/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests for Bug 1261299
 * Test that the service menu code path is called properly and the
 * current selection (transferable) is cached properly on the parent process.
 */

add_task(function* test_content_and_chrome_selection() {
  let testPage =
    'data:text/html,' +
    '<textarea id="textarea">Write something here</textarea>';
  let DOMWindowUtils = EventUtils._getDOMWindowUtils(window);
  let selectedText;

  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, testPage);
  yield BrowserTestUtils.synthesizeMouse("#textarea", 0, 0, {}, gBrowser.selectedBrowser);
  yield BrowserTestUtils.synthesizeKey("KEY_ArrowRight",
      {shiftKey: true, ctrlKey: true, code: "ArrowRight"}, gBrowser.selectedBrowser);
  selectedText = DOMWindowUtils.GetSelectionAsPlaintext();
  is(selectedText, "Write something here", "The macOS services got the selected content text");

  gURLBar.value = "test.mozilla.org";
  yield gURLBar.focus();
  yield BrowserTestUtils.synthesizeKey("KEY_ArrowRight",
      {shiftKey: true, ctrlKey: true, code: "ArrowRight"}, gBrowser.selectedBrowser);
  selectedText = DOMWindowUtils.GetSelectionAsPlaintext();
  is(selectedText, "test.mozilla.org", "The macOS services got the selected chrome text");

  yield BrowserTestUtils.removeTab(tab);
});

// Test switching active selection.
// Each tab has a content selection and when you switch to that tab, its selection becomes
// active aka the current selection.
// Expect: The active selection is what is being sent to OSX service menu.

add_task(function* test_active_selection_switches_properly() {
  let testPage1 =
    'data:text/html,' +
    '<textarea id="textarea">Write something here</textarea>';
  let testPage2 =
    'data:text/html,' +
    '<textarea id="textarea">Nothing available</textarea>';
  let DOMWindowUtils = EventUtils._getDOMWindowUtils(window);
  let selectedText;

  let tab1 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, testPage1);
  yield BrowserTestUtils.synthesizeMouse("#textarea", 0, 0, {}, gBrowser.selectedBrowser);
  yield BrowserTestUtils.synthesizeKey("KEY_ArrowRight",
      {shiftKey: true, ctrlKey: true, code: "ArrowRight"}, gBrowser.selectedBrowser);

  let tab2 = yield BrowserTestUtils.openNewForegroundTab(gBrowser, testPage2);
  yield BrowserTestUtils.synthesizeMouse("#textarea", 0, 0, {}, gBrowser.selectedBrowser);
  yield BrowserTestUtils.synthesizeKey("KEY_ArrowRight",
      {shiftKey: true, ctrlKey: true, code: "ArrowRight"}, gBrowser.selectedBrowser);

  yield BrowserTestUtils.switchTab(gBrowser, tab1);
  selectedText = DOMWindowUtils.GetSelectionAsPlaintext();
  is(selectedText, "Write something here", "The macOS services got the selected content text");

  yield BrowserTestUtils.switchTab(gBrowser, tab2);
  selectedText = DOMWindowUtils.GetSelectionAsPlaintext();
  is(selectedText, "Nothing available", "The macOS services got the selected content text");

  yield BrowserTestUtils.removeTab(tab1);
  yield BrowserTestUtils.removeTab(tab2);
});
