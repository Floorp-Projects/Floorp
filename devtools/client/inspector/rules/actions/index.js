/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createEnum } = require("devtools/client/shared/enum");

createEnum([

  // Disables all the pseudo class checkboxes because the current selection is not an
  // element node.
  "DISABLE_ALL_PSEUDO_CLASSES",

  // Sets the entire pseudo class state with the new list of applied pseudo-class
  // locks.
  "SET_PSEUDO_CLASSES",

  // Toggles on or off the given pseudo class value for the current selected element.
  "TOGGLE_PSEUDO_CLASS",

  // Updates whether or not the add new rule button should be enabled.
  "UPDATE_ADD_RULE_ENABLED",

  // Updates the entire class list state with the new list of classes.
  "UPDATE_CLASSES",

  // Updates whether or not the class list panel is expanded.
  "UPDATE_CLASS_PANEL_EXPANDED",

  // Updates the highlighted selector.
  "UPDATE_HIGHLIGHTED_SELECTOR",

  // Updates whether or not the print simulation button is hidden.
  "UPDATE_PRINT_SIMULATION_HIDDEN",

  // Updates the rules state with the new list of CSS rules for the selected element.
  "UPDATE_RULES",

  // Updates whether or not the source links are enabled.
  "UPDATE_SOURCE_LINK_ENABLED",

  // Updates the source link information for a given rule.
  "UPDATE_SOURCE_LINK",

], module.exports);
