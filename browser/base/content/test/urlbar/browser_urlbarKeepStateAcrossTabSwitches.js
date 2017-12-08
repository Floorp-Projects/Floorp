"use strict";

/**
 * Verify user typed text remains in the URL bar when tab switching, even when
 * loads fail.
 */
add_task(async function() {
  let input = "i-definitely-dont-exist.example.com";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank", false);
  let browser = tab.linkedBrowser;
  // NB: CPOW usage because new tab pages can be preloaded, in which case no
  // load events fire.
  // eslint-disable-next-line mozilla/no-cpows-in-tests
  await BrowserTestUtils.waitForCondition(() => browser.contentDocumentAsCPOW && !browser.contentDocumentAsCPOW.hidden);
  let errorPageLoaded = BrowserTestUtils.waitForErrorPage(tab.linkedBrowser);
  gURLBar.value = input;
  gURLBar.select();
  EventUtils.sendKey("return");
  await errorPageLoaded;
  is(gURLBar.textValue, input, "Text is still in URL bar");
  await BrowserTestUtils.switchTab(gBrowser, tab.previousSibling);
  await BrowserTestUtils.switchTab(gBrowser, tab);
  is(gURLBar.textValue, input, "Text is still in URL bar after tab switch");
  await BrowserTestUtils.removeTab(tab);
});

/**
 * Invalid URIs fail differently (that is, immediately, in the loadURI call)
 * if keyword searches are turned off. Test that this works, too.
 */
add_task(async function() {
  let input = "To be or not to be-that is the question";
  await SpecialPowers.pushPrefEnv({set: [["keyword.enabled", false]]});
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank", false);
  let browser = tab.linkedBrowser;
  // NB: CPOW usage because new tab pages can be preloaded, in which case no
  // load events fire.
  // eslint-disable-next-line mozilla/no-cpows-in-tests
  await BrowserTestUtils.waitForCondition(() => browser.contentDocumentAsCPOW && !browser.contentDocumentAsCPOW.hidden);
  let errorPageLoaded = BrowserTestUtils.waitForErrorPage(tab.linkedBrowser);
  gURLBar.value = input;
  gURLBar.select();
  EventUtils.sendKey("return");
  await errorPageLoaded;
  is(gURLBar.textValue, input, "Text is still in URL bar");
  is(tab.linkedBrowser.userTypedValue, input, "Text still stored on browser");
  await BrowserTestUtils.switchTab(gBrowser, tab.previousSibling);
  await BrowserTestUtils.switchTab(gBrowser, tab);
  is(gURLBar.textValue, input, "Text is still in URL bar after tab switch");
  is(tab.linkedBrowser.userTypedValue, input, "Text still stored on browser");
  await BrowserTestUtils.removeTab(tab);
});

