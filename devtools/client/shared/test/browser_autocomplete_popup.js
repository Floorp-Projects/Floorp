/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  const AutocompletePopup = require("resource://devtools/client/shared/autocomplete-popup.js");

  info("Create an autocompletion popup");
  const { doc } = await createHost();
  const input = doc.createElement("input");
  doc.body.appendChild(input);

  const autocompleteOptions = {
    position: "top",
    autoSelect: true,
  };
  const popup = new AutocompletePopup(doc, autocompleteOptions);
  input.focus();

  const items = [
    { label: "item0", value: "value0" },
    { label: "item1", value: "value1" },
    { label: "item2", value: "value2" },
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
  is(
    JSON.stringify(popup.getItems()),
    JSON.stringify(items),
    "getItems returns back the same items"
  );
  is(popup.selectedIndex, 0, "Index of the first item from top is selected.");
  is(popup.selectedItem, items[0], "First item from top is selected");
  // Make sure the list containing the active descendant doesn't get rebuilt
  // when the selected item changes.
  const listClone = getListFromActiveDescendant(popup, input);
  checkActiveDescendant(popup, input, listClone);

  popup.selectItemAtIndex(1);

  is(popup.selectedIndex, 1, "index 1 is selected");
  is(popup.selectedItem, items[1], "item1 is selected");
  checkActiveDescendant(popup, input, listClone);

  popup.selectedItem = items[2];

  is(popup.selectedIndex, 2, "index 2 is selected");
  is(popup.selectedItem, items[2], "item2 is selected");
  checkActiveDescendant(popup, input, listClone);

  is(popup.selectPreviousItem(), items[1], "selectPreviousItem() works");

  is(popup.selectedIndex, 1, "index 1 is selected");
  is(popup.selectedItem, items[1], "item1 is selected");
  checkActiveDescendant(popup, input, listClone);

  is(popup.selectNextItem(), items[2], "selectNextItem() works");

  is(popup.selectedIndex, 2, "index 2 is selected");
  is(popup.selectedItem, items[2], "item2 is selected");
  checkActiveDescendant(popup, input, listClone);

  ok(popup.selectNextItem(), "selectNextItem() works");

  is(popup.selectedIndex, 0, "index 0 is selected");
  is(popup.selectedItem, items[0], "item0 is selected");
  checkActiveDescendant(popup, input, listClone);

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

function getListFromActiveDescendant(popup, input) {
  const activeElement = input.ownerDocument.activeElement;
  const descendantId = activeElement.getAttribute("aria-activedescendant");
  const cloneItem = input.ownerDocument.querySelector("#" + descendantId);
  return cloneItem.parentNode;
}

function checkActiveDescendant(popup, input, list) {
  const activeElement = input.ownerDocument.activeElement;
  const descendantId = activeElement.getAttribute("aria-activedescendant");
  const popupItem = popup._tooltip.panel.querySelector("#" + descendantId);
  const cloneItem = input.ownerDocument.querySelector("#" + descendantId);

  ok(popupItem, "Active descendant is found in the popup list");
  ok(cloneItem, "Active descendant is found in the list clone");
  is(
    cloneItem.parentNode,
    list,
    "Active descendant is a child of the expected list"
  );
  is(
    stripNS(popupItem.outerHTML),
    cloneItem.outerHTML,
    "Cloned item has the same HTML as the original element"
  );
}
