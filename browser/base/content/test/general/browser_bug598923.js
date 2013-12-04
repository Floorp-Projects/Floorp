/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Test:
// * if add-on is installed to the add-on bar, the bar is made visible.
// * if add-on is uninstalled from the add-on bar, and no more add-ons there,
//   the bar is hidden.

function test() {
  let aml = AddonsMgrListener;
  ok(aml, "AddonsMgrListener exists");
  // check is hidden
  is(aml.addonBar.collapsed, true, "add-on bar is hidden initially");
  // aob gets the count
  AddonsMgrListener.onInstalling();
  // add an item
  let element = document.createElement("toolbaritem");
  element.id = "bug598923-addon-item";
  aml.addonBar.appendChild(element);
  // aob checks the count, makes visible
  AddonsMgrListener.onInstalled();
  // check is visible
  is(aml.addonBar.collapsed, false, "add-on bar has been made visible");
  // aob gets the count
  AddonsMgrListener.onUninstalling();
  // remove an item
  aml.addonBar.removeChild(element);
  // aob checks the count, makes hidden
  AddonsMgrListener.onUninstalled();
  // check is hidden
  is(aml.addonBar.collapsed, true, "add-on bar is hidden again");
}
