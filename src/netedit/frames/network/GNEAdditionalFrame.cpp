/****************************************************************************/
// Eclipse SUMO, Simulation of Urban MObility; see https://eclipse.org/sumo
// Copyright (C) 2001-2022 German Aerospace Center (DLR) and others.
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License 2.0 which is available at
// https://www.eclipse.org/legal/epl-2.0/
// This Source Code may also be made available under the following Secondary
// Licenses when the conditions for such availability set forth in the Eclipse
// Public License 2.0 are satisfied: GNU General Public License, version 2
// or later which is available at
// https://www.gnu.org/licenses/old-licenses/gpl-2.0-standalone.html
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
/****************************************************************************/
/// @file    GNEAdditionalFrame.cpp
/// @author  Pablo Alvarez Lopez
/// @date    Dec 2015
///
// The Widget for add additional elements
/****************************************************************************/
#include <config.h>

#include <netedit/GNEApplicationWindow.h>
#include <netedit/GNELane2laneConnection.h>
#include <netedit/GNENet.h>
#include <netedit/GNEViewNet.h>
#include <netedit/GNEViewParent.h>
#include <netedit/elements/additional/GNEAdditionalHandler.h>
#include <netedit/elements/network/GNEConnection.h>
#include <utils/gui/div/GLHelper.h>
#include <utils/gui/div/GUIDesigns.h>
#include <utils/gui/globjects/GLIncludes.h>
#include <utils/gui/windows/GUIAppEnum.h>
#include <utils/xml/SUMOSAXAttributesImpl_Cached.h>

#include "GNEAdditionalFrame.h"


// ===========================================================================
// method definitions
// ===========================================================================

GNEAdditionalFrame::GNEAdditionalFrame(FXHorizontalFrame* horizontalFrameParent, GNEViewNet* viewNet) :
    GNEFrame(horizontalFrameParent, viewNet, "Additionals"),
    myBaseAdditional(nullptr) {

    // create item Selector modul for additionals
    myAdditionalTagSelector = new GNEFrameModules::TagSelector(this, GNETagProperties::TagType::ADDITIONALELEMENT, SUMO_TAG_BUS_STOP);

    // Create additional parameters
    myAdditionalAttributes = new GNEFrameAttributeModules::AttributesCreator(this);

    // Create Netedit parameter
    myNeteditAttributes = new GNEFrameAttributeModules::NeteditAttributes(this);

    // Create consecutive Lane Selector
    mySelectorLaneParents = new GNECommonNetworkModules::SelectorParentLanes(this);

    // Create selector parent
    mySelectorAdditionalParent = new GNEFrameModules::SelectorParent(this);

    // Create selector child edges
    mySelectorChildEdges = new GNECommonNetworkModules::SelectorChildEdges(this);

    // Create selector child lanes
    mySelectorChildLanes = new GNECommonNetworkModules::SelectorChildLanes(this);

    // Create list for E2Multilane lane selector
    myE2MultilaneLaneSelector = new GNECommonNetworkModules::E2MultilaneLaneSelector(this);
}


GNEAdditionalFrame::~GNEAdditionalFrame() {
    // check if we have to delete base additional object
    if (myBaseAdditional) {
        delete myBaseAdditional;
    }
}


void
GNEAdditionalFrame::show() {
    // refresh tag selector
    myAdditionalTagSelector->refreshTagSelector();
    // show frame
    GNEFrame::show();
}


bool
GNEAdditionalFrame::addAdditional(const GNEViewNetHelper::ObjectsUnderCursor& objectsUnderCursor) {
    // first check that current selected additional is valid
    if (myAdditionalTagSelector->getCurrentTemplateAC() == nullptr) {
        myViewNet->setStatusBarText("Current selected additional isn't valid.");
        return false;
    }
    // show warning dialogbox and stop check if input parameters are valid
    if (!myAdditionalAttributes->areValuesValid()) {
        myAdditionalAttributes->showWarningMessage();
        return false;
    }
    // obtain tagproperty (only for improve code legibility)
    const auto& tagProperties = myAdditionalTagSelector->getCurrentTemplateAC()->getTagProperty();
    // create base additional
    if (!createBaseAdditionalObject(tagProperties)) {
        return false;
    }
    // obtain attributes and values
    myAdditionalAttributes->getAttributesAndValues(myBaseAdditional, true);
    // fill netedit attributes
    if (!myNeteditAttributes->getNeteditAttributesAndValues(myBaseAdditional, objectsUnderCursor.getLaneFront())) {
        return false;
    }
    // If consecutive Lane Selector is enabled, it means that either we're selecting lanes or we're finished or we'rent started
    if (tagProperties.hasAttribute(SUMO_ATTR_EDGE) || (tagProperties.getTag() == SUMO_TAG_VAPORIZER)) {
        return buildAdditionalOverEdge(objectsUnderCursor.getLaneFront(), tagProperties);
    } else if (tagProperties.hasAttribute(SUMO_ATTR_LANE)) {
        return buildAdditionalOverLane(objectsUnderCursor.getLaneFront(), tagProperties);
    } else if (tagProperties.getTag() == GNE_TAG_E2DETECTOR_MULTILANE) {
        return myE2MultilaneLaneSelector->addLane(objectsUnderCursor.getLaneFront());
    } else {
        return buildAdditionalOverView(tagProperties);
    }
}


void
GNEAdditionalFrame::showSelectorChildLanesModule() {
    // Show frame
    GNEFrame::show();
    // Update UseSelectedLane CheckBox
    mySelectorChildEdges->updateUseSelectedEdges();
    // Update UseSelectedLane CheckBox
    mySelectorChildLanes->updateUseSelectedLanes();
}


GNECommonNetworkModules::SelectorParentLanes*
GNEAdditionalFrame::getConsecutiveLaneSelector() const {
    return mySelectorLaneParents;
}


GNECommonNetworkModules::E2MultilaneLaneSelector*
GNEAdditionalFrame::getE2MultilaneLaneSelector() const {
    return myE2MultilaneLaneSelector;
}


void
GNEAdditionalFrame::createPath() {
    // obtain tagproperty (only for improve code legibility)
    const auto& tagProperty = myAdditionalTagSelector->getCurrentTemplateAC()->getTagProperty();
    // first check that current tag is valid (currently only for E2 multilane detectors)
    if (tagProperty.getTag() == GNE_TAG_E2DETECTOR_MULTILANE) {
        // now check number of lanes
        if (myE2MultilaneLaneSelector->getLanePath().size() < 2) {
            WRITE_WARNING("E2 multilane detectors need at least two consecutive lanes");
        } else if (createBaseAdditionalObject(tagProperty)) {
            // get attributes and values
            myAdditionalAttributes->getAttributesAndValues(myBaseAdditional, true);
            // fill netedit attributes
            if (myNeteditAttributes->getNeteditAttributesAndValues(myBaseAdditional, nullptr)) {
                // Check if ID has to be generated
                if (!myBaseAdditional->hasStringAttribute(SUMO_ATTR_ID)) {
                    myBaseAdditional->addStringAttribute(SUMO_ATTR_ID, myViewNet->getNet()->getAttributeCarriers()->generateAdditionalID(tagProperty.getTag()));
                }
                // obtain lane IDs
                std::vector<std::string> laneIDs;
                for (const auto& lane : myE2MultilaneLaneSelector->getLanePath()) {
                    laneIDs.push_back(lane.first->getID());
                }
                myBaseAdditional->addStringListAttribute(SUMO_ATTR_LANES, laneIDs);
                // set positions
                myBaseAdditional->addDoubleAttribute(SUMO_ATTR_POSITION, myE2MultilaneLaneSelector->getLanePath().front().second);
                myBaseAdditional->addDoubleAttribute(SUMO_ATTR_ENDPOS, myE2MultilaneLaneSelector->getLanePath().back().second);
                // parse common attributes
                if (buildAdditionalCommonAttributes(tagProperty)) {
                    // show warning dialogbox and stop check if input parameters are valid
                    if (myAdditionalAttributes->areValuesValid() == false) {
                        myAdditionalAttributes->showWarningMessage();
                    } else {
                        // declare additional handler
                        GNEAdditionalHandler additionalHandler(getViewNet()->getNet(), true);
                        // build additional
                        additionalHandler.parseSumoBaseObject(myBaseAdditional);
                        // Refresh additional Parent Selector (For additionals that have a limited number of children)
                        mySelectorAdditionalParent->refreshSelectorParentModule();
                        // abort E2 creation
                        myE2MultilaneLaneSelector->abortPathCreation();
                        // refresh additional attributes
                        myAdditionalAttributes->refreshAttributesCreator();
                    }
                }
            }
        }
    }
}


void 
GNEAdditionalFrame::stopConsecutiveLaneSelector() {
    // obtain tagproperty (only for improve code legibility)
    const auto& tagProperty = myAdditionalTagSelector->getCurrentTemplateAC()->getTagProperty();
    // abort if there isn't at least two lanes
    if (mySelectorLaneParents->getSelectedLanes().size() < 2) {
        WRITE_WARNING(tagProperty.getTagStr() + " requires at least two lanes.");
        // abort consecutive lane selector
        mySelectorLaneParents->abortConsecutiveLaneSelector();
    } else if (createBaseAdditionalObject(tagProperty)) {
        // get attributes and values
        myAdditionalAttributes->getAttributesAndValues(myBaseAdditional, true);
        // fill valuesOfElement with Netedit attributes from Frame
        myNeteditAttributes->getNeteditAttributesAndValues(myBaseAdditional, nullptr);
        // Check if ID has to be generated
        if (!myBaseAdditional->hasStringAttribute(SUMO_ATTR_ID)) {
            myBaseAdditional->addStringAttribute(SUMO_ATTR_ID, getViewNet()->getNet()->getAttributeCarriers()->generateAdditionalID(tagProperty.getTag()));
        }
        // obtain lane IDs
        std::vector<std::string> laneIDs;
        for (const auto& selectedlane : mySelectorLaneParents->getSelectedLanes()) {
            laneIDs.push_back(selectedlane.first->getID());
        }
        myBaseAdditional->addStringListAttribute(SUMO_ATTR_LANES, laneIDs);
        // Obtain clicked position over first lane
        myBaseAdditional->addDoubleAttribute(SUMO_ATTR_POSITION, mySelectorLaneParents->getSelectedLanes().front().second);
        // Obtain clicked position over last lane
        myBaseAdditional->addDoubleAttribute(SUMO_ATTR_ENDPOS, mySelectorLaneParents->getSelectedLanes().back().second);
        // parse common attributes
        if (buildAdditionalCommonAttributes(tagProperty)) {
            // show warning dialogbox and stop check if input parameters are valid
            if (myAdditionalAttributes->areValuesValid() == false) {
                myAdditionalAttributes->showWarningMessage();
            } else {
                // declare additional handler
                GNEAdditionalHandler additionalHandler(getViewNet()->getNet(), true);
                // build additional
                additionalHandler.parseSumoBaseObject(myBaseAdditional);
                // abort consecutive lane selector
                mySelectorLaneParents->abortConsecutiveLaneSelector();
                // refresh additional attributes
                myAdditionalAttributes->refreshAttributesCreator();
            }
        }
    }
}


void
GNEAdditionalFrame::tagSelected() {
    if (myAdditionalTagSelector->getCurrentTemplateAC()) {
        // show additional attributes modul
        myAdditionalAttributes->showAttributesCreatorModule(myAdditionalTagSelector->getCurrentTemplateAC(), {});
        // show netedit attributes
        myNeteditAttributes->showNeteditAttributesModule(myAdditionalTagSelector->getCurrentTemplateAC()->getTagProperty());
        // Show myAdditionalFrameParent if we're adding an slave element
        if (myAdditionalTagSelector->getCurrentTemplateAC()->getTagProperty().isChild()) {
            mySelectorAdditionalParent->showSelectorParentModule(myAdditionalTagSelector->getCurrentTemplateAC()->getTagProperty().getParentTags());
        } else {
            mySelectorAdditionalParent->hideSelectorParentModule();
        }
        // Show SelectorChildEdges if we're adding an additional that own the attribute SUMO_ATTR_EDGES
        if (myAdditionalTagSelector->getCurrentTemplateAC()->getTagProperty().hasAttribute(SUMO_ATTR_EDGES)) {
            mySelectorChildEdges->showSelectorChildEdgesModule();
        } else {
            mySelectorChildEdges->hideSelectorChildEdgesModule();
        }
        // check if we must show E2 multilane lane selector
        if (myAdditionalTagSelector->getCurrentTemplateAC()->getTagProperty().getTag() == GNE_TAG_E2DETECTOR_MULTILANE) {
            myE2MultilaneLaneSelector->showE2MultilaneLaneSelectorModule();
        } else if (myAdditionalTagSelector->getCurrentTemplateAC()->getTagProperty().hasAttribute(SUMO_ATTR_LANES)) {
            myE2MultilaneLaneSelector->hideE2MultilaneLaneSelectorModule();
            // Show SelectorChildLanes or consecutive lane selector if we're adding an additional that own the attribute SUMO_ATTR_LANES
            if (myAdditionalTagSelector->getCurrentTemplateAC()->getTagProperty().isChild() &&
                    (myAdditionalTagSelector->getCurrentTemplateAC()->getTagProperty().getParentTags().front() == SUMO_TAG_LANE)) {
                // show selector parent lane and hide selector child lane
                mySelectorLaneParents->showSelectorParentLanesModule();
                mySelectorChildLanes->hideSelectorChildLanesModule();
            } else {
                // show selector child lane and hide selector parent lane
                mySelectorChildLanes->showSelectorChildLanesModule();
                mySelectorLaneParents->hideSelectorParentLanesModule();
            }
        } else {
            myE2MultilaneLaneSelector->hideE2MultilaneLaneSelectorModule();
            mySelectorChildLanes->hideSelectorChildLanesModule();
            mySelectorLaneParents->hideSelectorParentLanesModule();
        }
    } else {
        // hide all moduls if additional isn't valid
        myAdditionalAttributes->hideAttributesCreatorModule();
        myNeteditAttributes->hideNeteditAttributesModule();
        mySelectorAdditionalParent->hideSelectorParentModule();
        mySelectorChildEdges->hideSelectorChildEdgesModule();
        mySelectorChildLanes->hideSelectorChildLanesModule();
        mySelectorLaneParents->hideSelectorParentLanesModule();
        myE2MultilaneLaneSelector->hideE2MultilaneLaneSelectorModule();
    }
}


bool
GNEAdditionalFrame::createBaseAdditionalObject(const GNETagProperties& tagProperty) {
    // check if baseAdditional exist, and if yes, delete it
    if (myBaseAdditional) {
        // go to base additional root
        while (myBaseAdditional->getParentSumoBaseObject()) {
            myBaseAdditional = myBaseAdditional->getParentSumoBaseObject();
        }
        // delete baseAdditional (and all children)
        delete myBaseAdditional;
        // reset baseAdditional
        myBaseAdditional = nullptr;
    }
    // declare tag for base additional
    SumoXMLTag baseAdditionalTag = tagProperty.getTag();
    // check if baseAdditionalTag has to be updated
    if (baseAdditionalTag == GNE_TAG_E2DETECTOR_MULTILANE) {
        baseAdditionalTag = SUMO_TAG_E2DETECTOR;
    } else if (baseAdditionalTag == GNE_TAG_CALIBRATOR_FLOW) {
        baseAdditionalTag = SUMO_TAG_FLOW;
    }
    // check if additional is child
    if (tagProperty.isChild()) {
        // get additional under cursor
        const GNEAdditional* additionalUnderCursor = myViewNet->getObjectsUnderCursor().getAdditionalFront();
        // if user click over an additional element parent, mark int in ParentAdditionalSelector
        if (additionalUnderCursor && (additionalUnderCursor->getTagProperty().getTag() == tagProperty.getParentTags().front())) {
            // update parent additional selected
            mySelectorAdditionalParent->setIDSelected(additionalUnderCursor->getID());
        }
        // stop if currently there isn't a valid selected parent
        if (mySelectorAdditionalParent->getIdSelected().empty()) {
            myAdditionalAttributes->showWarningMessage("A " + toString(tagProperty.getParentTags().front()) + " must be selected before insertion of " + myAdditionalTagSelector->getCurrentTemplateAC()->getTagProperty().getTagStr() + ".");
            return false;
        } else {
            // create baseAdditional parent
            myBaseAdditional = new CommonXMLStructure::SumoBaseObject(nullptr);
            // set parent tag
            myBaseAdditional->setTag(tagProperty.getParentTags().front());
            // add ID
            myBaseAdditional->addStringAttribute(SUMO_ATTR_ID, mySelectorAdditionalParent->getIdSelected());
            // create baseAdditional again as child of current myBaseAdditional
            myBaseAdditional = new CommonXMLStructure::SumoBaseObject(myBaseAdditional);
        }
    } else {
        // just create a base additional
        myBaseAdditional = new CommonXMLStructure::SumoBaseObject(nullptr);
    }
    // set baseAdditional tag
    myBaseAdditional->setTag(baseAdditionalTag);
    // BaseAdditional created, then return true
    return true;
}


bool
GNEAdditionalFrame::buildAdditionalCommonAttributes(const GNETagProperties& tagProperties) {
    // If additional has a interval defined by a begin or end, check that is valid
    if (tagProperties.hasAttribute(SUMO_ATTR_STARTTIME) && tagProperties.hasAttribute(SUMO_ATTR_END)) {
        const double begin = myBaseAdditional->getDoubleAttribute(SUMO_ATTR_STARTTIME);
        const double end = myBaseAdditional->getDoubleAttribute(SUMO_ATTR_END);
        if (begin > end) {
            myAdditionalAttributes->showWarningMessage("Attribute '" + toString(SUMO_ATTR_STARTTIME) + "' cannot be greater than attribute '" + toString(SUMO_ATTR_END) + "'.");
            return false;
        }
    }
    // If additional own the attribute SUMO_ATTR_FILE but was't defined, will defined as <ID>.xml
    if (tagProperties.hasAttribute(SUMO_ATTR_FILE) && myBaseAdditional->getStringAttribute(SUMO_ATTR_FILE).empty()) {
        if ((myAdditionalTagSelector->getCurrentTemplateAC()->getTagProperty().getTag() != SUMO_TAG_CALIBRATOR) && (myAdditionalTagSelector->getCurrentTemplateAC()->getTagProperty().getTag() != SUMO_TAG_REROUTER)) {
            // SUMO_ATTR_FILE is optional for calibrators and rerouters (fails to load in sumo when given and the file does not exist)
            myBaseAdditional->addStringAttribute(SUMO_ATTR_FILE, myBaseAdditional->getStringAttribute(SUMO_ATTR_ID) + ".xml");
        }
    }
    // check edge children
    if (tagProperties.hasAttribute(SUMO_ATTR_EDGES) && (!myBaseAdditional->hasStringListAttribute(SUMO_ATTR_EDGES) || myBaseAdditional->getStringListAttribute(SUMO_ATTR_EDGES).empty())) {
        // obtain edge IDs
        myBaseAdditional->addStringListAttribute(SUMO_ATTR_EDGES, mySelectorChildEdges->getEdgeIdsSelected());
        // check if attribute has at least one edge
        if (myBaseAdditional->getStringListAttribute(SUMO_ATTR_EDGES).empty()) {
            myAdditionalAttributes->showWarningMessage("List of " + toString(SUMO_TAG_EDGE) + "s cannot be empty");
            return false;
        }
    }
    // check lane children
    if (tagProperties.hasAttribute(SUMO_ATTR_LANES) && (!myBaseAdditional->hasStringListAttribute(SUMO_ATTR_LANES) || myBaseAdditional->getStringListAttribute(SUMO_ATTR_LANES).empty())) {
        // obtain lane IDs
        myBaseAdditional->addStringListAttribute(SUMO_ATTR_LANES, mySelectorChildLanes->getLaneIdsSelected());
        // check if attribute has at least one lane
        if (myBaseAdditional->getStringListAttribute(SUMO_ATTR_LANES).empty()) {
            myAdditionalAttributes->showWarningMessage("List of " + toString(SUMO_TAG_LANE) + "s cannot be empty");
            return false;
        }
    }
    // all ok, continue building additional
    return true;
}


bool
GNEAdditionalFrame::buildAdditionalOverEdge(GNELane* lane, const GNETagProperties& tagProperties) {
    // check that lane exist
    if (lane) {
        // Get attribute lane's edge
        myBaseAdditional->addStringAttribute(SUMO_ATTR_EDGE, lane->getParentEdge()->getID());
        // Check if ID has to be generated
        if (tagProperties.getTag() == SUMO_TAG_VAPORIZER) {
            myBaseAdditional->addStringAttribute(SUMO_ATTR_ID, lane->getParentEdge()->getID());
        } else if (!myBaseAdditional->hasStringAttribute(SUMO_ATTR_ID)) {
            myBaseAdditional->addStringAttribute(SUMO_ATTR_ID, myViewNet->getNet()->getAttributeCarriers()->generateAdditionalID(tagProperties.getTag()));
        }
    } else {
        return false;
    }
    // parse common attributes
    if (!buildAdditionalCommonAttributes(tagProperties)) {
        return false;
    }
    // show warning dialogbox and stop check if input parameters are valid
    if (!myAdditionalAttributes->areValuesValid()) {
        myAdditionalAttributes->showWarningMessage();
        return false;
    } else {
        // declare additional handler
        GNEAdditionalHandler additionalHandler(myViewNet->getNet(), true);
        // build additional
        additionalHandler.parseSumoBaseObject(myBaseAdditional);
        // Refresh additional Parent Selector (For additionals that have a limited number of children)
        mySelectorAdditionalParent->refreshSelectorParentModule();
        // clear selected eddges and lanes
        mySelectorChildEdges->onCmdClearSelection(nullptr, 0, nullptr);
        mySelectorChildLanes->onCmdClearSelection(nullptr, 0, nullptr);
        // refresh additional attributes
        myAdditionalAttributes->refreshAttributesCreator();
        return true;
    }
}


bool
GNEAdditionalFrame::buildAdditionalOverLane(GNELane* lane, const GNETagProperties& tagProperties) {
    // check that lane exist
    if (lane != nullptr) {
        // Get attribute lane
        myBaseAdditional->addStringAttribute(SUMO_ATTR_LANE, lane->getID());
        // Check if ID has to be generated
        if (!myBaseAdditional->hasStringAttribute(SUMO_ATTR_ID)) {
            myBaseAdditional->addStringAttribute(SUMO_ATTR_ID, myViewNet->getNet()->getAttributeCarriers()->generateAdditionalID(tagProperties.getTag()));
        }
    } else {
        return false;
    }
    // Obtain position of the mouse over lane (limited over grid)
    const double mousePositionOverLane = lane->getLaneShape().nearest_offset_to_point2D(myViewNet->snapToActiveGrid(myViewNet->getPositionInformation())) / lane->getLengthGeometryFactor();
    // set attribute position as mouse position over lane
    myBaseAdditional->addDoubleAttribute(SUMO_ATTR_POSITION, mousePositionOverLane);
    // parse common attributes
    if (!buildAdditionalCommonAttributes(tagProperties)) {
        return false;
    }
    // show warning dialogbox and stop check if input parameters are valid
    if (!myAdditionalAttributes->areValuesValid()) {
        myAdditionalAttributes->showWarningMessage();
        return false;
    } else {
        // declare additional handler
        GNEAdditionalHandler additionalHandler(myViewNet->getNet(), true);
        // build additional
        additionalHandler.parseSumoBaseObject(myBaseAdditional);
        // Refresh additional Parent Selector (For additionals that have a limited number of children)
        mySelectorAdditionalParent->refreshSelectorParentModule();
        // clear selected eddges and lanes
        mySelectorChildEdges->onCmdClearSelection(nullptr, 0, nullptr);
        mySelectorChildLanes->onCmdClearSelection(nullptr, 0, nullptr);
        // refresh additional attributes
        myAdditionalAttributes->refreshAttributesCreator();
        return true;
    }
}


bool
GNEAdditionalFrame::buildAdditionalOverView(const GNETagProperties& tagProperties) {
    // disable intervals (temporal)
    if ((tagProperties.getTag() == SUMO_TAG_INTERVAL) ||
        (tagProperties.getTag() == SUMO_TAG_DEST_PROB_REROUTE) ||
        (tagProperties.getTag() == SUMO_TAG_CLOSING_REROUTE) ||
        (tagProperties.getTag() == SUMO_TAG_CLOSING_LANE_REROUTE) ||
        (tagProperties.getTag() == SUMO_TAG_ROUTE_PROB_REROUTE) ||
        (tagProperties.getTag() == SUMO_TAG_PARKING_AREA_REROUTE)) {
        WRITE_WARNING("Currently unsuported. Create rerouter elements using rerouter dialog");
        return false;
    }
        // disable intervals (temporal)
    if (tagProperties.getTag() == SUMO_TAG_STEP) {
        WRITE_WARNING("Currently unsuported. Create VSS steps elements using VSS dialog");
        return false;
    }
    // Check if ID has to be generated
    if (!myBaseAdditional->hasStringAttribute(SUMO_ATTR_ID)) {
        myBaseAdditional->addStringAttribute(SUMO_ATTR_ID, myViewNet->getNet()->getAttributeCarriers()->generateAdditionalID(tagProperties.getTag()));
    }
    // Obtain position as the clicked position over view
    const Position viewPos = myViewNet->snapToActiveGrid(myViewNet->getPositionInformation());
    // add position and X-Y-Z attributes
    myBaseAdditional->addPositionAttribute(SUMO_ATTR_POSITION, viewPos);
    myBaseAdditional->addDoubleAttribute(SUMO_ATTR_X, viewPos.x());
    myBaseAdditional->addDoubleAttribute(SUMO_ATTR_Y, viewPos.y());
    myBaseAdditional->addDoubleAttribute(SUMO_ATTR_Z, viewPos.z());
    // parse common attributes
    if (!buildAdditionalCommonAttributes(tagProperties)) {
        return false;
    }
    // special case for VSS Steps
    if (myBaseAdditional->getTag() == SUMO_TAG_STEP) {
        // get VSS parent
        const auto VSSParent = myViewNet->getNet()->getAttributeCarriers()->retrieveAdditional(SUMO_TAG_VSS,
                               myBaseAdditional->getParentSumoBaseObject()->getStringAttribute(SUMO_ATTR_ID));
        // get last step
        GNEAdditional* step = nullptr;
        for (const auto& additionalChild : VSSParent->getChildAdditionals()) {
            if (!additionalChild->getTagProperty().isSymbol()) {
                step = additionalChild;
            }
        }
        // set time
        if (step) {
            myBaseAdditional->addTimeAttribute(SUMO_ATTR_TIME, string2time(step->getAttribute(SUMO_ATTR_TIME)) + TIME2STEPS(900));
        } else {
            myBaseAdditional->addTimeAttribute(SUMO_ATTR_TIME, 0);
        }
    }
    // show warning dialogbox and stop check if input parameters are valid
    if (myAdditionalAttributes->areValuesValid() == false) {
        myAdditionalAttributes->showWarningMessage();
        return false;
    } else {
        // declare additional handler
        GNEAdditionalHandler additionalHandler(myViewNet->getNet(), true);
        // build additional
        additionalHandler.parseSumoBaseObject(myBaseAdditional);
        // Refresh additional Parent Selector (For additionals that have a limited number of children)
        mySelectorAdditionalParent->refreshSelectorParentModule();
        // clear selected eddges and lanes
        mySelectorChildEdges->onCmdClearSelection(nullptr, 0, nullptr);
        mySelectorChildLanes->onCmdClearSelection(nullptr, 0, nullptr);
        // refresh additional attributes
        myAdditionalAttributes->refreshAttributesCreator();
        return true;
    }
}

/****************************************************************************/
