"use strict";

/**
 * Verify user typed text remains in the URL bar when tab switching, even when
 * loads fail.
 */
add_task(function* () {
  let input = "i-definitely-dont-exist.example.com";
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:newtab", false);
  // NB: CPOW usage because new tab pages can be preloaded, in which case no
  // load events fire.
  yield BrowserTestUtils.waitForCondition(() => !tab.linkedBrowser.contentDocument.hidden)
  let errorPageLoaded = BrowserTestUtils.waitForErrorPage(tab.linkedBrowser);
  gURLBar.value = input;
  gURLBar.select();
  EventUtils.sendKey("return");
  yield errorPageLoaded;
  is(gURLBar.textValue, input, "Text is still in URL bar");
  yield BrowserTestUtils.switchTab(gBrowser, tab.previousSibling);
  yield BrowserTestUtils.switchTab(gBrowser, tab);
  is(gURLBar.textValue, input, "Text is still in URL bar after tab switch");
  yield BrowserTestUtils.removeTab(tab);
});

/**
 * Invalid URIs fail differently (that is, immediately, in the loadURI call)
 * if keyword searches are turned off. Test that this works, too.
 */
add_task(function* () {
  let input = "To be or not to be-that is the question";
  yield new Promise(resolve => SpecialPowers.pushPrefEnv({set: [["keyword.enabled", false]]}, resolve));
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:newtab", false);
  // NB: CPOW usage because new tab pages can be preloaded, in which case no
  // load events fire.
  yield BrowserTestUtils.waitForCondition(() => !tab.linkedBrowser.contentDocument.hidden)
  let errorPageLoaded = BrowserTestUtils.waitForErrorPage(tab.linkedBrowser);
  gURLBar.value = input;
  gURLBar.select();
  EventUtils.sendKey("return");
  yield errorPageLoaded;
  is(gURLBar.textValue, input, "Text is still in URL bar");
  is(tab.linkedBrowser.userTypedValue, input, "Text still stored on browser");
  yield BrowserTestUtils.switchTab(gBrowser, tab.previousSibling);
  yield BrowserTestUtils.switchTab(gBrowser, tab);
  is(gURLBar.textValue, input, "Text is still in URL bar after tab switch");
  is(tab.linkedBrowser.userTypedValue, input, "Text still stored on browser");
  yield BrowserTestUtils.removeTab(tab);
});

