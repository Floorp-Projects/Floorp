/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verify user typed text remains in the URL bar when tab switching, even when
 * loads fail.
 */
add_task(async function() {
  let input = "i-definitely-dont-exist.example.com";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank",
    false
  );
  let browser = tab.linkedBrowser;
  // Note: Waiting on content document not being hidden because new tab pages can be preloaded,
  // in which case no load events fire.
  await ContentTask.spawn(browser, null, async () => {
    await ContentTaskUtils.waitForCondition(() => {
      return content.document && !content.document.hidden;
    });
  });
  let errorPageLoaded = BrowserTestUtils.waitForErrorPage(tab.linkedBrowser);
  gURLBar.value = input;
  gURLBar.select();
  EventUtils.sendKey("return");
  await errorPageLoaded;
  is(gURLBar.value, input, "Text is still in URL bar");
  await BrowserTestUtils.switchTab(gBrowser, tab.previousElementSibling);
  await BrowserTestUtils.switchTab(gBrowser, tab);
  is(gURLBar.value, input, "Text is still in URL bar after tab switch");
  BrowserTestUtils.removeTab(tab);
});

/**
 * Invalid URIs fail differently (that is, immediately, in the loadURI call)
 * if keyword searches are turned off. Test that this works, too.
 */
add_task(async function() {
  let input = "To be or not to be-that is the question";
  await SpecialPowers.pushPrefEnv({ set: [["keyword.enabled", false]] });
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank",
    false
  );
  let browser = tab.linkedBrowser;
  // Note: Waiting on content document not being hidden because new tab pages can be preloaded,
  // in which case no load events fire.
  await ContentTask.spawn(browser, null, async () => {
    await ContentTaskUtils.waitForCondition(() => {
      return content.document && !content.document.hidden;
    });
  });
  let errorPageLoaded = BrowserTestUtils.waitForErrorPage(tab.linkedBrowser);
  gURLBar.value = input;
  gURLBar.select();
  EventUtils.sendKey("return");
  await errorPageLoaded;
  is(gURLBar.value, input, "Text is still in URL bar");
  is(tab.linkedBrowser.userTypedValue, input, "Text still stored on browser");
  await BrowserTestUtils.switchTab(gBrowser, tab.previousElementSibling);
  await BrowserTestUtils.switchTab(gBrowser, tab);
  is(gURLBar.value, input, "Text is still in URL bar after tab switch");
  is(tab.linkedBrowser.userTypedValue, input, "Text still stored on browser");
  BrowserTestUtils.removeTab(tab);
});
