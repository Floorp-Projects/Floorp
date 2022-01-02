/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the urlbar value when focusing after pasting value.

const TEST_DATA = [
  {
    input: "this is a\ntest",
    expected: "this is a test",
  },
  {
    input: "http:\n//\nexample.\ncom",
    expected: "http://example.com",
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
    input: "test",
    expected: "test",
  },
];

add_task(async function test_paste_then_focus() {
  for (const testData of TEST_DATA) {
    gURLBar.value = "";
    gURLBar.focus();

    EventUtils.synthesizeKey("x");
    gURLBar.select();

    await paste(testData.input);

    gURLBar.blur();
    gURLBar.focus();

    Assert.equal(
      gURLBar.value,
      testData.expected,
      "Value on urlbar is correct"
    );
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
