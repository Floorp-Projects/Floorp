/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that properties can be selected and copied from the computed view.

const osString = Services.appinfo.OS;

const TEST_URI = `
  <style type="text/css">
    span {
      font-variant-caps: small-caps;
      color: #000000;
    }
    .nomatches {
      color: #ff0000;
    }
  </style>
  <div id="first" style="margin: 10em;
    font-size: 14pt; font-family: helvetica, sans-serif; color: #AAA">
    <h1>Some header text</h1>
    <p id="salutation" style="font-size: 12pt">hi.</p>
    <p id="body" style="font-size: 12pt">I am a test-case. This text exists
    solely to provide some things to <span style="color: yellow">
    highlight</span> and <span style="font-weight: bold">count</span>
    style list-items in the box at right. If you are reading this,
    you should go do something else instead. Maybe read a book. Or better
    yet, write some test-cases for another bit of code.
    <span style="font-style: italic">some text</span></p>
    <p id="closing">more text</p>
    <p>even more text</p>
  </div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openComputedView();
  yield selectNode("span", inspector);
  yield checkCopySelection(view);
  yield checkSelectAll(view);
});

function* checkCopySelection(view) {
  info("Testing selection copy");

  let contentDocument = view.styleDocument;
  let props = contentDocument.querySelectorAll(".property-view");
  ok(props, "captain, we have the property-view nodes");

  let range = contentDocument.createRange();
  range.setStart(props[1], 0);
  range.setEnd(props[3], 2);
  contentDocument.defaultView.getSelection().addRange(range);

  info("Checking that cssHtmlTree.siBoundCopy() returns the correct " +
    "clipboard value");

  let expectedPattern = "font-family: helvetica,sans-serif;[\\r\\n]+" +
                        "font-size: 16px;[\\r\\n]+" +
                        "font-variant-caps: small-caps;[\\r\\n]*";

  try {
    yield waitForClipboard(() => fireCopyEvent(props[0]),
                           () => checkClipboardData(expectedPattern));
  } catch (e) {
    failedClipboard(expectedPattern);
  }
}

function* checkSelectAll(view) {
  info("Testing select-all copy");

  let contentDoc = view.styleDocument;
  let prop = contentDoc.querySelector(".property-view");

  info("Checking that _onSelectAll() then copy returns the correct " +
    "clipboard value");
  view._contextmenu._onSelectAll();
  let expectedPattern = "color: rgb\\(255, 255, 0\\);[\\r\\n]+" +
                        "font-family: helvetica,sans-serif;[\\r\\n]+" +
                        "font-size: 16px;[\\r\\n]+" +
                        "font-variant-caps: small-caps;[\\r\\n]*";

  try {
    yield waitForClipboard(() => fireCopyEvent(prop),
                           () => checkClipboardData(expectedPattern));
  } catch (e) {
    failedClipboard(expectedPattern);
  }
}

function checkClipboardData(expectedPattern) {
  let actual = SpecialPowers.getClipboardData("text/unicode");
  let expectedRegExp = new RegExp(expectedPattern, "g");
  return expectedRegExp.test(actual);
}

function failedClipboard(expectedPattern) {
  // Format expected text for comparison
  let terminator = osString == "WINNT" ? "\r\n" : "\n";
  expectedPattern = expectedPattern.replace(/\[\\r\\n\][+*]/g, terminator);
  expectedPattern = expectedPattern.replace(/\\\(/g, "(");
  expectedPattern = expectedPattern.replace(/\\\)/g, ")");

  let actual = SpecialPowers.getClipboardData("text/unicode");

  // Trim the right hand side of our strings. This is because expectedPattern
  // accounts for windows sometimes adding a newline to our copied data.
  expectedPattern = expectedPattern.trimRight();
  actual = actual.trimRight();

  dump("TEST-UNEXPECTED-FAIL | Clipboard text does not match expected ... " +
    "results (escaped for accurate comparison):\n");
  info("Actual: " + escape(actual));
  info("Expected: " + escape(expectedPattern));
}
