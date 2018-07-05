/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  const AutocompletePopup = require("devtools/client/shared/autocomplete-popup");

  info("Create an autocompletion popup");
  const [,, doc] = await createHost();
  const input = doc.createElement("input");
  doc.body.appendChild(input);

  const autocompleteOptions = {
    position: "top",
    autoSelect: true
  };
  const popup = new AutocompletePopup(doc, autocompleteOptions);
  input.focus();

  const items = [
    {label: "item0", value: "value0"},
    {label: "item1", value: "value1"},
    {label: "item2", value: "value2"},
  ];

  ok(!popup.isOpen, "popup is not open");
  ok(!input.hasAttribute("aria-activedescendant"), "no aria-activedescendant");

  const onPopupOpen = popup.once("popup-opened");
  popup.openPopup(input);
  await onPopupOpen;

  ok(popup.isOpen, "popup is open");
  is(popup.itemCount, 0, "no items");
  ok(!input.hasAttribute("aria-activedescendant"), "no aria-activedescendant");

  popup.setItems(items);

  is(popup.itemCount, items.length, "items added");
  is(JSON.stringify(popup.getItems()), JSON.stringify(items),
    "getItems returns back the same items");
  is(popup.selectedIndex, 2, "Index of the first item from bottom is selected.");
  is(popup.selectedItem, items[2], "First item from bottom is selected");
  checkActiveDescendant(popup, input);

  popup.selectedIndex = 1;

  is(popup.selectedIndex, 1, "index 1 is selected");
  is(popup.selectedItem, items[1], "item1 is selected");
  checkActiveDescendant(popup, input);

  popup.selectedItem = items[2];

  is(popup.selectedIndex, 2, "index 2 is selected");
  is(popup.selectedItem, items[2], "item2 is selected");
  checkActiveDescendant(popup, input);

  is(popup.selectPreviousItem(), items[1], "selectPreviousItem() works");

  is(popup.selectedIndex, 1, "index 1 is selected");
  is(popup.selectedItem, items[1], "item1 is selected");
  checkActiveDescendant(popup, input);

  is(popup.selectNextItem(), items[2], "selectNextItem() works");

  is(popup.selectedIndex, 2, "index 2 is selected");
  is(popup.selectedItem, items[2], "item2 is selected");
  checkActiveDescendant(popup, input);

  ok(popup.selectNextItem(), "selectNextItem() works");

  is(popup.selectedIndex, 0, "index 0 is selected");
  is(popup.selectedItem, items[0], "item0 is selected");
  checkActiveDescendant(popup, input);

  items.push({label: "label3", value: "value3"});
  popup.appendItem(items[3]);

  is(popup.itemCount, items.length, "item3 appended");

  popup.selectedIndex = 3;
  is(popup.selectedItem, items[3], "item3 is selected");
  checkActiveDescendant(popup, input);

  popup.removeItem(items[2]);

  is(popup.selectedIndex, 2, "index2 is selected");
  is(popup.selectedItem, items[3], "item3 is still selected");
  checkActiveDescendant(popup, input);
  is(popup.itemCount, items.length - 1, "item2 removed");

  popup.clearItems();
  is(popup.itemCount, 0, "items cleared");
  ok(!input.hasAttribute("aria-activedescendant"), "no aria-activedescendant");

  const onPopupClose = popup.once("popup-closed");
  popup.hidePopup();
  await onPopupClose;
});

function stripNS(text) {
  return text.replace(RegExp(' xmlns="http://www.w3.org/1999/xhtml"', "g"), "");
}

function checkActiveDescendant(popup, input) {
  const activeElement = input.ownerDocument.activeElement;
  const descendantId = activeElement.getAttribute("aria-activedescendant");
  const popupItem = popup._tooltip.panel.querySelector("#" + descendantId);
  const cloneItem = input.ownerDocument.querySelector("#" + descendantId);

  ok(popupItem, "Active descendant is found in the popup list");
  ok(cloneItem, "Active descendant is found in the list clone");
  is(stripNS(popupItem.outerHTML), cloneItem.outerHTML,
    "Cloned item has the same HTML as the original element");
}
