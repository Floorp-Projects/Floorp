/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Make this available to both AMD and CJS environments
define(function(require, exports, module) {
  // Simplied selector targetting elements that can receive the focus, full
  // version at
  // http://stackoverflow.com/questions/1599660/which-html-elements-can-receive-focus
  // .
  module.exports.focusableSelector = [
    "a[href]:not([tabindex='-1'])",
    "button:not([disabled]):not([tabindex='-1'])",
    "iframe:not([tabindex='-1'])",
    "input:not([disabled]):not([tabindex='-1'])",
    "select:not([disabled]):not([tabindex='-1'])",
    "textarea:not([disabled]):not([tabindex='-1'])",
    "[tabindex]:not([tabindex='-1'])",
  ].join(", ");
});
