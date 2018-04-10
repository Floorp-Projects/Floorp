/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createEnum } = require("devtools/client/shared/enum");

createEnum([

  // Toggle the visibiltiy of the font editor
  "UPDATE_EDITOR_VISIBILITY",

  // Update the list of fonts.
  "UPDATE_FONTS",

  // Update the preview text.
  "UPDATE_PREVIEW_TEXT",

], module.exports);
