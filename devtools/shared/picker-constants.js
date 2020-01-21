/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Types of content pickers that can be triggered from DevTools.
 * Used for instance by RDM to keep track of which picker is currently enabled.
 */
module.exports = {
  ELEMENT: "ELEMENT",
  EYEDROPPER: "EYEDROPPER",
};
