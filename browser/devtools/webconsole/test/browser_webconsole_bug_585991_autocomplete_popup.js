/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URI = "data:text/html;charset=utf-8,<p>bug 585991 - autocomplete popup test";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(HUD) {
  let items = [
    {label: "item0", value: "value0"},
    {label: "item1", value: "value1"},
    {label: "item2", value: "value2"},
  ];

  let popup = HUD.jsterm.autocompletePopup;

  let input = popup._document.activeElement;
  function getActiveDescendant() {
    return input.ownerDocument.getElementById(
      input.getAttribute("aria-activedescendant"));
  }

  ok(!popup.isOpen, "popup is not open");
  ok(!input.hasAttribute("aria-activedescendant"), "no aria-activedescendant");

  popup._panel.addEventListener("popupshown", function() {
    popup._panel.removeEventListener("popupshown", arguments.callee, false);

    ok(popup.isOpen, "popup is open");

    is(popup.itemCount, 0, "no items");
    ok(!input.hasAttribute("aria-activedescendant"),
       "no aria-activedescendant");

    popup.setItems(items);

    is(popup.itemCount, items.length, "items added");

    let sameItems = popup.getItems();
    is(sameItems.every(function(aItem, aIndex) {
      return aItem === items[aIndex];
    }), true, "getItems returns back the same items");

    is(popup.selectedIndex, 2,
       "Index of the first item from bottom is selected.");
    is(popup.selectedItem, items[2], "First item from bottom is selected");
    ok(getActiveDescendant().selected, "aria-activedescendant is correct");

    popup.selectedIndex = 1;

    is(popup.selectedIndex, 1, "index 1 is selected");
    is(popup.selectedItem, items[1], "item1 is selected");
    ok(getActiveDescendant().selected, "aria-activedescendant is correct");

    popup.selectedItem = items[2];

    is(popup.selectedIndex, 2, "index 2 is selected");
    is(popup.selectedItem, items[2], "item2 is selected");
    ok(getActiveDescendant().selected, "aria-activedescendant is correct");

    is(popup.selectPreviousItem(), items[1], "selectPreviousItem() works");

    is(popup.selectedIndex, 1, "index 1 is selected");
    is(popup.selectedItem, items[1], "item1 is selected");
    ok(getActiveDescendant().selected, "aria-activedescendant is correct");

    is(popup.selectNextItem(), items[2], "selectPreviousItem() works");

    is(popup.selectedIndex, 2, "index 2 is selected");
    is(popup.selectedItem, items[2], "item2 is selected");
    ok(getActiveDescendant().selected, "aria-activedescendant is correct");

    ok(popup.selectNextItem(), "selectPreviousItem() works");

    is(popup.selectedIndex, 0, "index 0 is selected");
    is(popup.selectedItem, items[0], "item0 is selected");
    ok(getActiveDescendant().selected, "aria-activedescendant is correct");

    items.push({label: "label3", value: "value3"});
    popup.appendItem(items[3]);

    is(popup.itemCount, items.length, "item3 appended");

    popup.selectedIndex = 3;
    is(popup.selectedItem, items[3], "item3 is selected");
    ok(getActiveDescendant().selected, "aria-activedescendant is correct");

    popup.removeItem(items[2]);

    is(popup.selectedIndex, 2, "index2 is selected");
    is(popup.selectedItem, items[3], "item3 is still selected");
    ok(getActiveDescendant().selected, "aria-activedescendant is correct");
    is(popup.itemCount, items.length - 1, "item2 removed");

    popup.clearItems();
    is(popup.itemCount, 0, "items cleared");
    ok(!input.hasAttribute("aria-activedescendant"),
       "no aria-activedescendant");

    popup.hidePopup();
    finishTest();
  }, false);

  popup.openPopup();
}

