/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// Used in accessible component for properties tree rendering.
exports.TREE_ROW_HEIGHT = 21;

// Initial sidebar width.
exports.SIDEBAR_WIDTH = "350px";

// When value is updated either in the tree or sidebar.
exports.VALUE_FLASHING_DURATION = 500;
// When new row is selected, flash highlighter.
exports.VALUE_HIGHLIGHT_DURATION = 1000;

// If the panel width is smaller than given amount of pixels,
// the sidebar automatically switches from 'landscape' to 'portrait' mode.
exports.PORTRAIT_MODE_WIDTH = 700;

// Names for Redux actions.
exports.FETCH_CHILDREN = "FETCH_CHILDREN";
exports.UPDATE_DETAILS = "UPDATE_DETAILS";
exports.RESET = "RESET";
exports.SELECT = "SELECT";
exports.HIGHLIGHT = "HIGHLIGHT";
exports.UNHIGHLIGHT = "UNHIGHLIGHT";
exports.ENABLE = "ENABLE";
exports.DISABLE = "DISABLE";
exports.UPDATE_CAN_BE_DISABLED = "UPDATE_CAN_BE_DISABLED";
exports.UPDATE_CAN_BE_ENABLED = "UPDATE_CAN_BE_ENABLED";

// Ordered accessible properties to be displayed by the accessible component.
exports.ORDERED_PROPS = [
  "name",
  "role",
  "actions",
  "value",
  "DOMNode",
  "description",
  "keyboardShortcut",
  "childCount",
  "indexInParent",
  "states",
  "attributes"
];

// Accessible events (emitted by accessible front) that the accessible component
// listens to for a current accessible.
exports.ACCESSIBLE_EVENTS = [
  "actions-change",
  "attributes-change",
  "description-change",
  "name-change",
  "reorder",
  "shortcut-change",
  "states-change",
  "text-change",
  "value-change",
  "index-in-parent-change"
];

// Telemetry name constants.
exports.A11Y_SERVICE_DURATION = "DEVTOOLS_ACCESSIBILITY_SERVICE_TIME_ACTIVE_SECONDS";
exports.A11Y_SERVICE_ENABLED_COUNT = "devtools.accessibility.service_enabled_count";
