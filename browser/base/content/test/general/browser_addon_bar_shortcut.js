/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  let addonbar = document.getElementById("addon-bar");
  ok(addonbar.collapsed, "addon bar is collapsed by default");

  // show the add-on bar
  EventUtils.synthesizeKey("/", { accelKey: true }, window);
  ok(!addonbar.collapsed, "addon bar is not collapsed after toggle");

  // hide the add-on bar
  EventUtils.synthesizeKey("/", { accelKey: true }, window);

  // confirm addon bar is closed
  ok(addonbar.collapsed, "addon bar is collapsed after toggle");
}
