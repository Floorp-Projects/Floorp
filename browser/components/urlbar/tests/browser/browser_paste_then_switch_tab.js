/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the urlbar value when switching tab after pasting value.

const TEST_DATA = [
  {
    input: "this is a\ntest",
    expected: "this is a test",
  },
  {
    input: "https:\n//\nexample.\ncom",
    expected: UrlbarTestUtils.trimURL("https://example.com"),
  },
  {
    input: "http:\n//\nexample.\ncom",
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    expected: UrlbarTestUtils.trimURL("http://example.com"),
  },
  {
    input: "javasc\nript:\nalert(1)",
    expected: "alert(1)",
  },
  {
    input: "javascript:alert(1)",
    expected: "alert(1)",
  },
  {
    // Has U+3000 IDEOGRAPHIC SPACE.
    input: "Mozilla　Firefox",
    expected: "Mozilla Firefox",
  },
  {
    input: "test",
    expected: "test",
  },
];

add_task(async function test_paste_then_switch_tab() {
  for (const testData of TEST_DATA) {
    gURLBar.focus();
    gURLBar.select();

    await paste(testData.input);

    // Switch to a new tab.
    const originalTab = gBrowser.selectedTab;
    const newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    await BrowserTestUtils.waitForCondition(() => !gURLBar.value);

    // Switch back to original tab.
    gBrowser.selectedTab = originalTab;

    Assert.equal(
      gURLBar.value,
      testData.expected,
      "Value on urlbar is correct"
    );

    BrowserTestUtils.removeTab(newTab);
  }
});

async function paste(input) {
  await SimpleTest.promiseClipboardChange(input, () => {
    clipboardHelper.copyString(input);
  });

  document.commandDispatcher
    .getControllerForCommand("cmd_paste")
    .doCommand("cmd_paste");
}
