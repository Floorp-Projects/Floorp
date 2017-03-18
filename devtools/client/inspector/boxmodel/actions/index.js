/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createEnum } = require("devtools/client/shared/enum");

createEnum([

  // Update the geometry editor's enabled state.
  "UPDATE_GEOMETRY_EDITOR_ENABLED",

  // Update the layout state with the latest layout properties.
  "UPDATE_LAYOUT",

  // Update the offset parent state with the new DOM node.
  "UPDATE_OFFSET_PARENT",

], module.exports);
