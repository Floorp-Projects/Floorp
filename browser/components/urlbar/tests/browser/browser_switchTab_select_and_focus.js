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
    targetSelectionStart: 0,
    targetSelectionEnd: 0,
    anotherTab: userTypedTab,
  });
  await doTest({
    targetTab: webpageTab,
    targetSelectionStart: 2,
    targetSelectionEnd: 5,
    anotherTab: userTypedTab,
  });
  await doTest({
    targetTab: webpageTab,
    targetSelectionStart: webpageTabURL.length,
    targetSelectionEnd: webpageTabURL.length,
    anotherTab: userTypedTab,
  });
  await doTest({
    targetTab: webpageTab,
    targetSelectionStart: 0,
    targetSelectionEnd: 0,
    anotherTab: emptyTab,
  });
  await doTest({
    targetTab: userTypedTab,
    targetSelectionStart: 0,
    targetSelectionEnd: 0,
    anotherTab: webpageTab,
  });
  await doTest({
    targetTab: userTypedTab,
    targetSelectionStart: 0,
    targetSelectionEnd: 0,
    anotherTab: emptyTab,
  });
  await doTest({
    targetTab: userTypedTab,
    targetSelectionStart: 1,
    targetSelectionEnd: 2,
    anotherTab: emptyTab,
  });
  await doTest({
    targetTab: userTypedTab,
    targetSelectionStart: userTypedTabText.length,
    targetSelectionEnd: userTypedTabText.length,
    anotherTab: emptyTab,
  });
  await doTest({
    targetTab: emptyTab,
    targetSelectionStart: 0,
    targetSelectionEnd: 0,
    anotherTab: webpageTab,
  });
  await doTest({
    targetTab: emptyTab,
    targetSelectionStart: 0,
    targetSelectionEnd: 0,
    anotherTab: userTypedTab,
  });
});

async function doTest({
  targetTab,
  targetSelectionStart,
  targetSelectionEnd,
  anotherTab,
}) {
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
    gURLBar.inputField.setSelectionRange(
      targetSelectionStart,
      targetSelectionEnd
    );
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
      Assert.equal(gURLBar.selectionStart, targetSelectionStart);
      Assert.equal(gURLBar.selectionEnd, targetSelectionEnd);
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
