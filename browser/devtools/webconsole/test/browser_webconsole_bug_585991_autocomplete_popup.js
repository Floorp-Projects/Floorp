/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Web Console test suite.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mihai Sucan <mihai.sucan@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "data:text/html,<p>bug 585991 - autocomplete popup test";

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.gcli.enable");
});

function test() {
  Services.prefs.setBoolPref("devtools.gcli.enable", false);
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoaded, true);
}

function tabLoaded() {
  browser.removeEventListener("load", tabLoaded, true);
  openConsole();

  let items = [
    {label: "item0", value: "value0"},
    {label: "item1", value: "value1"},
    {label: "item2", value: "value2"},
  ];

  let hudId = HUDService.getHudIdByWindow(content);
  let HUD = HUDService.hudReferences[hudId];
  let popup = HUD.jsterm.autocompletePopup;

  ok(!popup.isOpen, "popup is not open");

  popup._panel.addEventListener("popupshown", function() {
    popup._panel.removeEventListener("popupshown", arguments.callee, false);

    ok(popup.isOpen, "popup is open");

    is(popup.itemCount, 0, "no items");

    popup.setItems(items);

    is(popup.itemCount, items.length, "items added");

    let sameItems = popup.getItems();
    is(sameItems.every(function(aItem, aIndex) {
      return aItem === items[aIndex];
    }), true, "getItems returns back the same items");

    is(popup.selectedIndex, -1, "no index is selected");
    ok(!popup.selectedItem, "no item is selected");

    popup.selectedIndex = 1;

    is(popup.selectedIndex, 1, "index 1 is selected");
    is(popup.selectedItem, items[1], "item1 is selected");

    popup.selectedItem = items[2];

    is(popup.selectedIndex, 2, "index 2 is selected");
    is(popup.selectedItem, items[2], "item2 is selected");

    is(popup.selectPreviousItem(), items[1], "selectPreviousItem() works");

    is(popup.selectedIndex, 1, "index 1 is selected");
    is(popup.selectedItem, items[1], "item1 is selected");

    is(popup.selectNextItem(), items[2], "selectPreviousItem() works");

    is(popup.selectedIndex, 2, "index 2 is selected");
    is(popup.selectedItem, items[2], "item2 is selected");

    ok(!popup.selectNextItem(), "selectPreviousItem() works");

    is(popup.selectedIndex, -1, "no index is selected");
    ok(!popup.selectedItem, "no item is selected");

    items.push({label: "label3", value: "value3"});
    popup.appendItem(items[3]);

    is(popup.itemCount, items.length, "item3 appended");

    popup.selectedIndex = 3;
    is(popup.selectedItem, items[3], "item3 is selected");

    popup.removeItem(items[2]);

    is(popup.selectedIndex, 2, "index2 is selected");
    is(popup.selectedItem, items[3], "item3 is still selected");
    is(popup.itemCount, items.length - 1, "item2 removed");

    popup.clearItems();
    is(popup.itemCount, 0, "items cleared");

    popup.hidePopup();
    finishTest();
  }, false);

  popup.openPopup();
}

