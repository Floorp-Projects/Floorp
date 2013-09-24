/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  let addonbar = document.getElementById("addon-bar");
  ok(addonbar.collapsed, "addon bar is collapsed by default");

  // make add-on bar visible
  setToolbarVisibility(addonbar, true);
  ok(!addonbar.collapsed, "addon bar is not collapsed after toggle");

  // click the close button
  let closeButton = document.getElementById("addonbar-closebutton");
  EventUtils.synthesizeMouseAtCenter(closeButton, {});

  // confirm addon bar is closed
  ok(addonbar.collapsed, "addon bar is collapsed after clicking close button");
}
