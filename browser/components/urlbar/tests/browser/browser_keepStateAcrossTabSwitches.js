/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verify user typed text remains in the URL bar when tab switching, even when
 * loads fail.
 */
add_task(async function validURL() {
  let input = "i-definitely-dont-exist.example.com";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/"
  );
  let browser = tab.linkedBrowser;
  // Note: Waiting on content document not being hidden because new tab pages can be preloaded,
  // in which case no load events fire.
  await SpecialPowers.spawn(browser, [], async () => {
    await ContentTaskUtils.waitForCondition(() => {
      return content.document && !content.document.hidden;
    });
  });
  let errorPageLoaded = BrowserTestUtils.waitForErrorPage(browser);
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
add_task(async function invalidURL() {
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
  await SpecialPowers.spawn(browser, [], async () => {
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

/**
 * Test whether the text will be selected properly when tab switching if the text
 * field had focused.
 */
add_task(async function select() {
  const tab1 = gBrowser.selectedTab;
  const tab1ExpectedResult = {
    value: "",
    textSelected: false,
  };

  const tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/"
  );
  const tab2ExpectedResult = {
    value: "example.com",
    textSelected: false,
  };

  const tab3 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.org/"
  );
  gURLBar.focus();
  const tab3ExpectedResult = {
    value: "example.org",
    textSelected: true,
  };

  await assertInputStatusAfterSwitchingTab(tab1, tab1ExpectedResult);
  await assertInputStatusAfterSwitchingTab(tab2, tab2ExpectedResult);
  await assertInputStatusAfterSwitchingTab(tab3, tab3ExpectedResult);
  await assertInputStatusAfterSwitchingTab(tab2, tab2ExpectedResult);
  await assertInputStatusAfterSwitchingTab(tab3, tab3ExpectedResult);

  BrowserTestUtils.removeTab(tab2);
  BrowserTestUtils.removeTab(tab3);
});

async function assertInputStatusAfterSwitchingTab(tab, expected) {
  await BrowserTestUtils.switchTab(gBrowser, tab);
  info(`Test for ${expected.value}`);
  is(gURLBar.value, expected.value, "Text is correct");
  if (expected.textSelected) {
    is(
      gURLBar.inputField.ownerDocument.activeElement,
      gURLBar.inputField,
      "The input field has focus"
    );
    is(gURLBar.inputField.selectionStart, 0, "selectionStart is correct");
    is(
      gURLBar.inputField.selectionEnd,
      expected.value.length,
      "selectionEnd is correct"
    );
  } else {
    isnot(
      gURLBar.inputField.ownerDocument.activeElement,
      gURLBar.inputField,
      "The input field does not have focus"
    );
  }
}
