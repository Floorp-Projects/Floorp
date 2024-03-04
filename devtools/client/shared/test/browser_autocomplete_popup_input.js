/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  // Prevent the URL Bar to steal the focus.
  const preventUrlBarFocus = e => {
    e.preventDefault();
  };
  window.gURLBar.addEventListener("beforefocus", preventUrlBarFocus);
  registerCleanupFunction(() => {
    window.gURLBar.removeEventListener("beforefocus", preventUrlBarFocus);
  });

  const AutocompletePopup = require("resource://devtools/client/shared/autocomplete-popup.js");

  info("Create an autocompletion popup and an input that will be bound to it");
  const { doc } = await createHost();

  const input = doc.createElement("input");
  const prevInput = doc.createElement("input");
  doc.body.append(prevInput, input, doc.createElement("input"));

  const onSelectCalled = [];
  const onClickCalled = [];
  const popup = new AutocompletePopup(doc, {
    input,
    position: "top",
    autoSelect: true,
    onSelect: item => onSelectCalled.push(item),
    onClick: (e, item) => onClickCalled.push(item),
  });

  input.focus();
  ok(hasFocus(input), "input has focus");

  info(
    "Check that Tab moves the focus out of the input when the popup isn't opened"
  );
  EventUtils.synthesizeKey("KEY_Tab");
  is(onClickCalled.length, 0, "onClick wasn't called");
  is(hasFocus(input), false, "input does not have the focus anymore");
  info("Set the focus back to the input and open the popup");
  input.focus();
  await new Promise(res => setTimeout(res, 0));
  ok(hasFocus(input), "input is focused");

  await populateAndOpenPopup(popup);

  const checkSelectedItem = (expected, info) =>
    checkPopupSelectedItem(popup, input, expected, info);

  checkSelectedItem(popupItems[0], "First item from top is selected");
  is(
    onSelectCalled[0].label,
    popupItems[0].label,
    "onSelect was called with expected param"
  );

  info("Check that arrow down/up navigates into the list");
  EventUtils.synthesizeKey("KEY_ArrowDown");
  checkSelectedItem(popupItems[1], "item-1 is selected");
  is(
    onSelectCalled[1].label,
    popupItems[1].label,
    "onSelect was called with expected param"
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  checkSelectedItem(popupItems[2], "item-2 is selected");
  is(
    onSelectCalled[2].label,
    popupItems[2].label,
    "onSelect was called with expected param"
  );

  EventUtils.synthesizeKey("KEY_ArrowDown");
  checkSelectedItem(popupItems[0], "item-0 is selected");
  is(
    onSelectCalled[3].label,
    popupItems[0].label,
    "onSelect was called with expected param"
  );

  EventUtils.synthesizeKey("KEY_ArrowUp");
  checkSelectedItem(popupItems[2], "item-2 is selected");
  is(
    onSelectCalled[4].label,
    popupItems[2].label,
    "onSelect was called with expected param"
  );

  EventUtils.synthesizeKey("KEY_ArrowUp");
  checkSelectedItem(popupItems[1], "item-2 is selected");
  is(
    onSelectCalled[5].label,
    popupItems[1].label,
    "onSelect was called with expected param"
  );

  info("Check that Escape closes the popup");
  let onPopupClosed = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_Escape");
  await onPopupClosed;
  ok(true, "popup was closed with Escape key");
  ok(hasFocus(input), "input still has the focus");
  is(onClickCalled.length, 0, "onClick wasn't called");

  info("Fill the input");
  const value = "item";
  EventUtils.sendString(value);
  is(input.value, value, "input has the expected value");
  is(
    input.selectionStart,
    value.length,
    "input cursor is at expected position"
  );
  info("Open the popup again");
  await populateAndOpenPopup(popup);

  info("Check that Arrow Left + Shift does not close the popup");
  const timeoutRes = "TIMED_OUT";
  const onRaceEnded = Promise.race([
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(res => setTimeout(() => res(timeoutRes), 500)),
    popup.once("popup-closed"),
  ]);
  EventUtils.synthesizeKey("KEY_ArrowLeft", { shiftKey: true });
  const raceResult = await onRaceEnded;
  is(raceResult, timeoutRes, "popup wasn't closed");
  ok(popup.isOpen, "popup is still open");
  is(input.selectionEnd - input.selectionStart, 1, "text was selected");
  ok(hasFocus(input), "input still has the focus");

  info("Check that Arrow Left closes the popup");
  onPopupClosed = popup.once("popup-closed");
  EventUtils.synthesizeKey("KEY_ArrowLeft");
  await onPopupClosed;
  is(
    input.selectionStart,
    value.length - 1,
    "input cursor was moved one char back"
  );
  is(input.selectionEnd, input.selectionStart, "selection was removed");
  is(onClickCalled.length, 0, "onClick wasn't called");
  ok(hasFocus(input), "input still has the focus");

  info("Open the popup again");
  await populateAndOpenPopup(popup);

  info("Check that Arrow Right + Shift does not trigger onClick");
  EventUtils.synthesizeKey("KEY_ArrowRight", { shiftKey: true });
  is(onClickCalled.length, 0, "onClick wasn't called");
  is(input.selectionEnd - input.selectionStart, 1, "input text was selected");
  ok(hasFocus(input), "input still has the focus");

  info("Check that Arrow Right triggers onClick");
  EventUtils.synthesizeKey("KEY_ArrowRight");
  is(onClickCalled.length, 1, "onClick was called");
  is(
    onClickCalled[0],
    popupItems[0],
    "onClick was called with the selected item"
  );
  ok(hasFocus(input), "input still has the focus");

  info("Check that Enter triggers onClick");
  EventUtils.synthesizeKey("KEY_Enter");
  is(onClickCalled.length, 2, "onClick was called");
  is(
    onClickCalled[1],
    popupItems[0],
    "onClick was called with the selected item"
  );
  ok(hasFocus(input), "input still has the focus");

  info("Check that Tab triggers onClick");
  EventUtils.synthesizeKey("KEY_Tab");
  is(onClickCalled.length, 3, "onClick was called");
  is(
    onClickCalled[2],
    popupItems[0],
    "onClick was called with the selected item"
  );
  ok(hasFocus(input), "input still has the focus");

  info(
    "Check that Shift+Tab does not trigger onClick and move the focus out of the input"
  );
  EventUtils.synthesizeKey("KEY_Tab", { shiftKey: true });
  is(onClickCalled.length, 3, "onClick wasn't called");

  is(hasFocus(input), false, "input does not have the focus anymore");
  is(hasFocus(prevInput), true, "Shift+Tab moves the focus to prevInput");

  const onPopupClose = popup.once("popup-closed");
  popup.hidePopup();
  await onPopupClose;
});

const popupItems = [
  { label: "item-0", value: "value-0" },
  { label: "item-1", value: "value-1" },
  { label: "item-2", value: "value-2" },
];

async function populateAndOpenPopup(popup) {
  popup.setItems(popupItems);
  await popup.openPopup();
}

/**
 * Returns true if the give node is currently focused.
 */
function hasFocus(node) {
  return (
    node.ownerDocument.activeElement == node && node.ownerDocument.hasFocus()
  );
}

/**
 * Check that the selected item in the popup is the expected one. Also check that the
 * active descendant is properly set and that the popup has the focus.
 *
 * @param {AutocompletePopup} popup
 * @param {HTMLInput} input
 * @param {Object} expectedSelectedItem
 * @param {String} info
 */
function checkPopupSelectedItem(popup, input, expectedSelectedItem, info) {
  is(popup.selectedItem.label, expectedSelectedItem.label, info);
  checkActiveDescendant(popup, input);
  ok(hasFocus(input), "input still has the focus");
}

function checkActiveDescendant(popup, input) {
  const activeElement = input.ownerDocument.activeElement;
  const descendantId = activeElement.getAttribute("aria-activedescendant");
  const popupItem = popup._tooltip.panel.querySelector(`#${descendantId}`);
  const cloneItem = input.ownerDocument.querySelector(`#${descendantId}`);

  ok(popupItem, "Active descendant is found in the popup list");
  ok(cloneItem, "Active descendant is found in the list clone");
  is(
    stripNS(popupItem.outerHTML),
    cloneItem.outerHTML,
    "Cloned item has the same HTML as the original element"
  );
}

function stripNS(text) {
  return text.replace(RegExp(' xmlns="http://www.w3.org/1999/xhtml"', "g"), "");
}
