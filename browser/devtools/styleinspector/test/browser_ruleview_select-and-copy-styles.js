/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that properties can be selected and copied from the rule view

XPCOMUtils.defineLazyGetter(this, "osString", function() {
  return Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS;
});

let test = asyncTest(function*() {
  yield addTab("data:text/html;charset=utf-8,<p>rule view context menu test</p>");

  info("Creating the test document");
  content.document.body.innerHTML = '<style type="text/css"> ' +
    'html { color: #000000; } ' +
    'span { font-variant: small-caps; color: #000000; } ' +
    '.nomatches {color: #ff0000;}</style> <div id="first" style="margin: 10em; ' +
    'font-size: 14pt; font-family: helvetica, sans-serif; color: #AAA">\n' +
    '<h1>Some header text</h1>\n' +
    '<p id="salutation" style="font-size: 12pt">hi.</p>\n' +
    '<p id="body" style="font-size: 12pt">I am a test-case. This text exists ' +
    'solely to provide some things to <span style="color: yellow">' +
    'highlight</span> and <span style="font-weight: bold">count</span> ' +
    'style list-items in the box at right. If you are reading this, ' +
    'you should go do something else instead. Maybe read a book. Or better ' +
    'yet, write some test-cases for another bit of code. ' +
    '<span style="font-style: italic">some text</span></p>\n' +
    '<p id="closing">more text</p>\n' +
    '<p>even more text</p>' +
    '</div>';
  content.document.title = "Rule view context menu test";

  info("Opening the computed view");
  let {toolbox, inspector, view} = yield openRuleView();

  info("Selecting the test node");
  yield selectNode("div", inspector);

  yield checkCopySelection(view);
  yield checkSelectAll(view);
});

function checkCopySelection(view) {
  info("Testing selection copy");

  let contentDoc = view.doc;
  let prop = contentDoc.querySelector(".ruleview-property");
  let values = contentDoc.querySelectorAll(".ruleview-propertycontainer");

  let range = contentDoc.createRange();
  range.setStart(prop, 0);
  range.setEnd(values[4], 2);
  let selection = view.doc.defaultView.getSelection().addRange(range);

  info("Checking that _Copy() returns the correct clipboard value");

  let expectedPattern = "    margin: 10em;[\\r\\n]+" +
                        "    font-size: 14pt;[\\r\\n]+" +
                        "    font-family: helvetica,sans-serif;[\\r\\n]+" +
                        "    color: #AAA;[\\r\\n]+" +
                        "}[\\r\\n]+" +
                        "html {[\\r\\n]+" +
                        "    color: #000;[\\r\\n]*";

  return waitForClipboard(() => {
    fireCopyEvent(prop);
  }, () => {
    return checkClipboardData(expectedPattern);
  }).then(() => {}, () => {
    failedClipboard(expectedPattern);
  });
}

function checkSelectAll(view) {
  info("Testing select-all copy");

  let contentDoc = view.doc;
  let prop = contentDoc.querySelector(".ruleview-property");

  info("Checking that _SelectAll() then copy returns the correct clipboard value");
  view._onSelectAll();
  let expectedPattern = "[\\r\\n]+" +
                        "element {[\\r\\n]+" +
                        "    margin: 10em;[\\r\\n]+" +
                        "    font-size: 14pt;[\\r\\n]+" +
                        "    font-family: helvetica,sans-serif;[\\r\\n]+" +
                        "    color: #AAA;[\\r\\n]+" +
                        "}[\\r\\n]+" +
                        "html {[\\r\\n]+" +
                        "    color: #000;[\\r\\n]+" +
                        "}[\\r\\n]*";

  return waitForClipboard(() => {
    fireCopyEvent(prop);
  }, () => {
    return checkClipboardData(expectedPattern);
  }).then(() => {}, () => {
    failedClipboard(expectedPattern);
  });
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
