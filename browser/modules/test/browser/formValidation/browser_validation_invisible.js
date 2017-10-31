"use strict";

var gInvalidFormPopup = document.getElementById("invalid-form-popup");

function checkPopupHide() {
  ok(gInvalidFormPopup.state != "showing" && gInvalidFormPopup.state != "open",
     "[Test " + testId + "] The invalid form popup should not be shown");
}

var testId = 0;

function incrementTest() {
  testId++;
  info("Starting next part of test");
}

/**
 * In this test, we check that no popup appears if the element display is none.
 */
add_task(async function test_display_none() {
  ok(gInvalidFormPopup,
     "The browser should have a popup to show when a form is invalid");

  incrementTest();
  let testPage =
    "data:text/html;charset=utf-8," +
    '<form target="t"><input type="url"  placeholder="url" value="http://" style="display: none;"><input id="s" type="button" value="check"></form>';
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, testPage);
  await BrowserTestUtils.synthesizeMouse("#s", 0, 0, {}, gBrowser.selectedBrowser);

  checkPopupHide();
  await BrowserTestUtils.removeTab(tab);
});

/**
 * In this test, we check that no popup appears if the element visibility is hidden.
 */
add_task(async function test_visibility_hidden() {
  incrementTest();
  let testPage =
    "data:text/html;charset=utf-8," +
    '<form target="t"><input type="url"  placeholder="url" value="http://" style="visibility: hidden;"><input id="s" type="button" value="check"></form>';
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, testPage);
  await BrowserTestUtils.synthesizeMouse("#s", 0, 0, {}, gBrowser.selectedBrowser);

  checkPopupHide();
  await BrowserTestUtils.removeTab(tab);
});

