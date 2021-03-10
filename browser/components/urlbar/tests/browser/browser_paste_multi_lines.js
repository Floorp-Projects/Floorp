/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test handling whitespace chars such as "\n”.

const TEST_DATA = [
  {
    input: "this is a\ntest",
    expected: "this is a test",
  },
  {
    input: "this is    a\n\ttest",
    expected: "this is    a  test",
  },
  {
    input: "http:\n//\nexample.\ncom",
    expected: "http://example.com",
  },
  {
    input: "htp:example.\ncom",
    expected: "htp:example.com",
  },
  {
    input: "example.\ncom",
    expected: "example.com",
  },
  {
    input: "http://example.com/foo       bar/",
    expected: "http://example.com/foo       bar/",
  },
  {
    input: "javasc\nript:\nalert(1)",
    expected: "alert(1)",
  },
  {
    input: "a\nb\nc",
    expected: "a b c",
  },
  {
    input: "lo\ncal\nhost",
    expected: "localhost",
  },
  {
    input: "data:text/html,<iframe\n src='example\n.com'>\n</iframe>",
    expected: "data:text/html,<iframe src='example.com'></iframe>",
  },
];

add_task(async function test_paste_multi_lines() {
  for (const testData of TEST_DATA) {
    gURLBar.value = "";
    gURLBar.focus();

    await paste(testData.input);

    Assert.equal(gURLBar.value, testData.expected, "Pasted value is correct");
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
