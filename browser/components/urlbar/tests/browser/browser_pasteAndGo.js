/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests for the paste and go functionality of the urlbar.
 */

add_task(async function() {
  const kURLs = [
    "http://example.com/1",
    "http://example.org/2\n",
    "http://\nexample.com/3\n",
  ];
  for (let url of kURLs) {
    await BrowserTestUtils.withNewTab("about:blank", async function(browser) {
      gURLBar.focus();

      await SimpleTest.promiseClipboardChange(url, () => {
        clipboardHelper.copyString(url);
      });
      let menuitem = await promiseContextualMenuitem("paste-and-go");
      let browserLoadedPromise = BrowserTestUtils.browserLoaded(
        browser,
        false,
        url.replace(/\n/g, "")
      );
      menuitem.closest("menupopup").activateItem(menuitem);
      // Using toSource in order to get the newlines escaped:
      info("Paste and go, loading " + url.toSource());
      await browserLoadedPromise;
      ok(true, "Successfully loaded " + url);
    });
  }
});

add_task(async function test_invisible_char() {
  const url = "http://example.com/4\u2028";
  await BrowserTestUtils.withNewTab("about:blank", async function(browser) {
    gURLBar.focus();
    await SimpleTest.promiseClipboardChange(url, () => {
      clipboardHelper.copyString(url);
    });
    let menuitem = await promiseContextualMenuitem("paste-and-go");
    let browserLoadedPromise = BrowserTestUtils.browserLoaded(
      browser,
      false,
      url.replace(/\u2028/g, "")
    );
    menuitem.closest("menupopup").activateItem(menuitem);
    // Using toSource in order to get the newlines escaped:
    info("Paste and go, loading " + url.toSource());
    await browserLoadedPromise;
    ok(true, "Successfully loaded " + url);
  });
});

add_task(async function test_with_input_and_results() {
  // Test paste and go When there's some input and the results pane is open.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "foo",
  });
  const url = "http://example.com/";
  await SimpleTest.promiseClipboardChange(url, () => {
    clipboardHelper.copyString(url);
  });
  let menuitem = await promiseContextualMenuitem("paste-and-go");
  let browserLoadedPromise = BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    url
  );
  menuitem.closest("menupopup").activateItem(menuitem);
  // Using toSource in order to get the newlines escaped:
  info("Paste and go, loading " + url.toSource());
  await browserLoadedPromise;
  ok(true, "Successfully loaded " + url);
});

async function promiseContextualMenuitem(anonid) {
  let textBox = gURLBar.querySelector("moz-input-box");
  let cxmenu = textBox.menupopup;
  let cxmenuPromise = BrowserTestUtils.waitForEvent(cxmenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(gURLBar.inputField, {
    type: "contextmenu",
    button: 2,
  });
  await cxmenuPromise;
  return textBox.getMenuItem(anonid);
}
