/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Checks that the status of text selection and focusing by tab switching.
 */

"use strict";

add_task(async function () {
  // Create a tab with normal web page.
  const webpageTabURL = "https://example.com";
  const webpageTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: webpageTabURL,
  });

  // Create a tab with userTypedValue.
  const userTypedTabText = "test";
  const userTypedTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
  });
  await UrlbarTestUtils.inputIntoURLBar(window, userTypedTabText);

  // Create an empty tab.
  const emptyTab = await BrowserTestUtils.openNewForegroundTab({ gBrowser });

  registerCleanupFunction(async () => {
    await PlacesUtils.history.clear();
    BrowserTestUtils.removeTab(webpageTab);
    BrowserTestUtils.removeTab(userTypedTab);
    BrowserTestUtils.removeTab(emptyTab);
  });

  await doTest({
    targetTab: webpageTab,
    anotherTab: userTypedTab,
  });
  await doTest({
    targetTab: webpageTab,
    anotherTab: emptyTab,
  });
  await doTest({
    targetTab: userTypedTab,
    anotherTab: webpageTab,
  });
  await doTest({
    targetTab: userTypedTab,
    anotherTab: emptyTab,
  });
  await doTest({
    targetTab: emptyTab,
    anotherTab: webpageTab,
  });
  await doTest({
    targetTab: emptyTab,
    anotherTab: userTypedTab,
  });
});

async function doTest({ targetTab, anotherTab }) {
  const testCases = [
    {
      targetFocus: false,
      anotherFocus: false,
    },
    {
      targetFocus: true,
      anotherFocus: false,
    },
    {
      targetFocus: true,
      anotherFocus: true,
    },
  ];

  for (const { targetFocus, anotherFocus } of testCases) {
    // Setup the target tab.
    await switchTab(targetTab);
    setURLBarFocus(targetFocus);
    const targetValue = gURLBar.value;

    // Switch to another tab.
    await switchTab(anotherTab);
    setURLBarFocus(anotherFocus);

    // Switch back to the target tab.
    await switchTab(targetTab);

    // Check whether the value, selection and focusing status are reverted.
    Assert.equal(gURLBar.value, targetValue);
    Assert.equal(gURLBar.focused, targetFocus);
    if (gURLBar.focused) {
      Assert.equal(gURLBar.selectionStart, 0);
      Assert.equal(gURLBar.selectionEnd, gURLBar.value.length);
    } else {
      Assert.equal(gURLBar.selectionStart, gURLBar.value.length);
      Assert.equal(gURLBar.selectionEnd, gURLBar.value.length);
    }
  }
}

function setURLBarFocus(focus) {
  if (focus) {
    gURLBar.focus();
  } else {
    gURLBar.blur();
  }
}

async function switchTab(tab) {
  if (gBrowser.selectedTab !== tab) {
    EventUtils.synthesizeMouseAtCenter(tab, {});
    await BrowserTestUtils.waitForCondition(() => gBrowser.selectedTab === tab);
  }
}
