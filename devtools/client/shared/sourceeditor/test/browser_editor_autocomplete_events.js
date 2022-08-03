/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "data:text/html;charset=UTF-8,<html><body><bar></bar>" +
  "<div id='baz'></div><body></html>";

add_task(async function() {
  await addTab(TEST_URI);
  await runTests();
});

async function runTests() {
  const target = await createAndAttachTargetForTab(gBrowser.selectedTab);
  const inspector = await target.getFront("inspector");
  const walker = inspector.walker;
  const { ed, win, edWin } = await setup({
    autocomplete: true,
    mode: Editor.modes.css,
    autocompleteOpts: {
      walker,
      cssProperties: getClientCssProperties(),
    },
  });
  await testMouse(ed, edWin);
  await testKeyboard(ed, edWin);
  await testKeyboardCycle(ed, edWin);
  await testKeyboardCycleForPrefixedString(ed, edWin);
  await testKeyboardCSSComma(ed, edWin);
  await testCloseOnEscape(ed, edWin);
  teardown(ed, win);
}

async function testKeyboard(ed, win) {
  ed.focus();
  ed.setText("b");
  ed.setCursor({ line: 1, ch: 1 });

  const popupOpened = ed.getAutocompletionPopup().once("popup-opened");

  const autocompleteKey = Editor.keyFor("autocompletion", {
    noaccel: true,
  }).toUpperCase();
  EventUtils.synthesizeKey("VK_" + autocompleteKey, { ctrlKey: true }, win);

  info("Waiting for popup to be opened");
  await popupOpened;

  EventUtils.synthesizeKey("VK_RETURN", {}, win);
  is(ed.getText(), "bar", "Editor text has been updated");
}

async function testKeyboardCycle(ed, win) {
  ed.focus();
  ed.setText("b");
  ed.setCursor({ line: 1, ch: 1 });

  const popupOpened = ed.getAutocompletionPopup().once("popup-opened");

  const autocompleteKey = Editor.keyFor("autocompletion", {
    noaccel: true,
  }).toUpperCase();
  EventUtils.synthesizeKey("VK_" + autocompleteKey, { ctrlKey: true }, win);

  info("Waiting for popup to be opened");
  await popupOpened;

  EventUtils.synthesizeKey("VK_DOWN", {}, win);
  is(ed.getText(), "bar", "Editor text has been updated");

  EventUtils.synthesizeKey("VK_DOWN", {}, win);
  is(ed.getText(), "body", "Editor text has been updated");

  EventUtils.synthesizeKey("VK_DOWN", {}, win);
  is(ed.getText(), "#baz", "Editor text has been updated");
}

async function testKeyboardCycleForPrefixedString(ed, win) {
  ed.focus();
  ed.setText("#b");
  ed.setCursor({ line: 1, ch: 2 });

  const popupOpened = ed.getAutocompletionPopup().once("popup-opened");

  const autocompleteKey = Editor.keyFor("autocompletion", {
    noaccel: true,
  }).toUpperCase();
  EventUtils.synthesizeKey("VK_" + autocompleteKey, { ctrlKey: true }, win);

  info("Waiting for popup to be opened");
  await popupOpened;

  EventUtils.synthesizeKey("VK_DOWN", {}, win);
  is(ed.getText(), "#baz", "Editor text has been updated");
}

async function testKeyboardCSSComma(ed, win) {
  ed.focus();
  ed.setText("b");
  ed.setCursor({ line: 1, ch: 1 });

  let isPopupOpened = false;
  const popupOpened = ed.getAutocompletionPopup().once("popup-opened");
  popupOpened.then(() => {
    isPopupOpened = true;
  });

  EventUtils.synthesizeKey(",", {}, win);

  await wait(500);

  ok(!isPopupOpened, "Autocompletion shouldn't be opened");
}

async function testMouse(ed, win) {
  ed.focus();
  ed.setText("b");
  ed.setCursor({ line: 1, ch: 1 });

  const popupOpened = ed.getAutocompletionPopup().once("popup-opened");

  const autocompleteKey = Editor.keyFor("autocompletion", {
    noaccel: true,
  }).toUpperCase();
  EventUtils.synthesizeKey("VK_" + autocompleteKey, { ctrlKey: true }, win);

  info("Waiting for popup to be opened");
  await popupOpened;
  ed.getAutocompletionPopup()._list.children[2].click();
  is(ed.getText(), "#baz", "Editor text has been updated");
}

async function testCloseOnEscape(ed, win) {
  ed.focus();
  ed.setText("b");
  ed.setCursor({ line: 1, ch: 1 });

  const popupOpened = ed.getAutocompletionPopup().once("popup-opened");

  const autocompleteKey = Editor.keyFor("autocompletion", {
    noaccel: true,
  }).toUpperCase();
  EventUtils.synthesizeKey("VK_" + autocompleteKey, { ctrlKey: true }, win);

  info("Waiting for popup to be opened");
  await popupOpened;

  is(ed.getAutocompletionPopup().isOpen, true, "The popup is open");

  const popupClosed = ed.getAutocompletionPopup().once("popup-closed");
  EventUtils.synthesizeKey("VK_ESCAPE", {}, win);

  await popupClosed;
  is(ed.getAutocompletionPopup().isOpen, false, "Escape key closed popup");
}
