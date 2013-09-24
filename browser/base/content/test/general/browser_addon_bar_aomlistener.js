/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {

  let addonbar = document.getElementById("addon-bar");
  ok(addonbar.collapsed, "addon bar is collapsed by default");

  function addItem(id) {
    let button = document.createElement("toolbarbutton");
    button.id = id;
    let palette = document.getElementById("navigator-toolbox").palette;
    palette.appendChild(button);
    addonbar.insertItem(id, null, null, false);
  }

  // call onInstalling
  AddonsMgrListener.onInstalling();

  // add item to the bar
  let id = "testbutton";
  addItem(id);

  // call onInstalled
  AddonsMgrListener.onInstalled();

  // confirm bar is visible
  ok(!addonbar.collapsed, "addon bar is not collapsed after toggle");

  // call onUninstalling
  AddonsMgrListener.onUninstalling();

  // remove item from the bar
  addonbar.currentSet = addonbar.currentSet.replace("," + id, "");

  // call onUninstalled
  AddonsMgrListener.onUninstalled();

  // confirm bar is not visible
  ok(addonbar.collapsed, "addon bar is collapsed after toggle");

  // call onEnabling
  AddonsMgrListener.onEnabling();

  // add item to the bar
  let id = "testbutton";
  addItem(id);

  // call onEnabled
  AddonsMgrListener.onEnabled();

  // confirm bar is visible
  ok(!addonbar.collapsed, "addon bar is not collapsed after toggle");

  // call onDisabling
  AddonsMgrListener.onDisabling();

  // remove item from the bar
  addonbar.currentSet = addonbar.currentSet.replace("," + id, "");

  // call onDisabled
  AddonsMgrListener.onDisabled();

  // confirm bar is not visible
  ok(addonbar.collapsed, "addon bar is collapsed after toggle");
}
