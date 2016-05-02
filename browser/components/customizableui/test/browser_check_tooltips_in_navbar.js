/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function* check_tooltips_in_navbar() {
  yield startCustomizing();
  let homeButtonWrapper = document.getElementById("wrapper-home-button");
  let homeButton = document.getElementById("home-button");
  is(homeButtonWrapper.getAttribute("tooltiptext"), homeButton.getAttribute("label"), "the wrapper's tooltip should match the button's label");
  ok(homeButtonWrapper.getAttribute("tooltiptext"), "the button should have tooltip text");
  yield endCustomizing();
});
