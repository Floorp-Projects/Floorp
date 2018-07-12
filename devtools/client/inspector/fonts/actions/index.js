/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createEnum } = require("devtools/client/shared/enum");

createEnum([

  // Update the custom font variation instance with the current axes values.
  "UPDATE_CUSTOM_INSTANCE",

  // Reset font editor to intial state.
  "RESET_EDITOR",

  // Apply the variation settings of a font instance.
  "APPLY_FONT_VARIATION_INSTANCE",

  // Update the value of a variable font axis.
  "UPDATE_AXIS_VALUE",

  // Update font editor with applicable fonts and user-defined CSS font properties.
  "UPDATE_EDITOR_STATE",

  // Toggle the visibiltiy of the font editor
  "UPDATE_EDITOR_VISIBILITY",

  // Update the list of fonts.
  "UPDATE_FONTS",

  // Update the preview text.
  "UPDATE_PREVIEW_TEXT",

  // Update the value of a CSS font property
  "UPDATE_PROPERTY_VALUE",

  // Update the warning message with the reason for not showing the font editor
  "UPDATE_WARNING_MESSAGE",

], module.exports);
