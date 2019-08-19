/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Ensures that pasting unsafe protocols in the urlbar have the protocol
 * correctly stripped.
 */

var pairs = [
  ["javascript:", ""],
  ["javascript:1+1", "1+1"],
  ["javascript:document.domain", "document.domain"],
  [
    " \u0001\u0002\u0003\u0004\u0005\u0006\u0007\u0008\u0009javascript:document.domain",
    "document.domain",
  ],
  ["java\nscript:foo", "foo"],
  ["java\tscript:foo", "foo"],
  ["http://\nexample.com", "http://example.com"],
  ["http://\nexample.com\n", "http://example.com"],
  ["data:text/html,<body>hi</body>", "data:text/html,<body>hi</body>"],
  ["javaScript:foopy", "foopy"],
  ["javaScript:javaScript:alert('hi')", "alert('hi')"],
  // Nested things get confusing because some things don't parse as URIs:
  ["javascript:javascript:alert('hi!')", "alert('hi!')"],
  [
    "data:data:text/html,<body>hi</body>",
    "data:data:text/html,<body>hi</body>",
  ],
  ["javascript:data:javascript:alert('hi!')", "data:javascript:alert('hi!')"],
  [
    "javascript:data:text/html,javascript:alert('hi!')",
    "data:text/html,javascript:alert('hi!')",
  ],
  [
    "data:data:text/html,javascript:alert('hi!')",
    "data:data:text/html,javascript:alert('hi!')",
  ],
];

let supportsNullBytes = AppConstants.platform == "macosx";
// Note that \u000d (\r) is missing here; we test it separately because it
// makes the test sad on Windows.
let gobbledygook =
  "\u000a\u000b\u000c\u000e\u000f\u0010\u0011\u0012\u0013\u0014javascript:foo";
if (supportsNullBytes) {
  gobbledygook = "\u0000" + gobbledygook;
}
pairs.push([gobbledygook, "foo"]);

let supportsReturnWithoutNewline = AppConstants.platform != "win";
if (supportsReturnWithoutNewline) {
  pairs.push(["java\rscript:foo", "foo"]);
}

var clipboardHelper = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(
  Ci.nsIClipboardHelper
);

async function paste(input) {
  try {
    await SimpleTest.promiseClipboardChange(input, () => {
      clipboardHelper.copyString(input);
    });
  } catch (ex) {
    Assert.ok(false, "Failed to copy string '" + input + "' to clipboard");
  }

  document.commandDispatcher
    .getControllerForCommand("cmd_paste")
    .doCommand("cmd_paste");
}

add_task(async function test_stripUnsafeProtocolPaste() {
  for (let [inputValue, expectedURL] of pairs) {
    gURLBar.value = "";
    gURLBar.focus();
    await paste(inputValue);

    Assert.equal(
      gURLBar.value,
      expectedURL,
      `entering ${inputValue} strips relevant bits.`
    );

    await new Promise(resolve => setTimeout(resolve, 0));
  }
});
