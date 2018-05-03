/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that properties can be selected and copied from the rule view

const osString = Services.appinfo.OS;

const TEST_URI = `
  <style type="text/css">
    html {
      color: #000000;
    }
    span {
      font-variant: small-caps; color: #000000;
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

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = await openRuleView();
  await selectNode("div", inspector);
  await checkCopySelection(view);
  await checkSelectAll(view);
  await checkCopyEditorValue(view);
});

async function checkCopySelection(view) {
  info("Testing selection copy");

  let contentDoc = view.styleDocument;
  let win = view.styleWindow;
  let prop = contentDoc.querySelector(".ruleview-property");
  let values = contentDoc.querySelectorAll(".ruleview-propertyvaluecontainer");

  let range = contentDoc.createRange();
  range.setStart(prop, 0);
  range.setEnd(values[4], 2);
  win.getSelection().addRange(range);
  info("Checking that _Copy() returns the correct clipboard value");

  let expectedPattern = "    margin: 10em;[\\r\\n]+" +
                        "    font-size: 14pt;[\\r\\n]+" +
                        "    font-family: helvetica, sans-serif;[\\r\\n]+" +
                        "    color: #AAA;[\\r\\n]+" +
                        "}[\\r\\n]+" +
                        "html {[\\r\\n]+" +
                        "    color: #000000;[\\r\\n]*";

  let allMenuItems = openStyleContextMenuAndGetAllItems(view, prop);
  let menuitemCopy = allMenuItems.find(item => item.label ===
    STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copy"));

  ok(menuitemCopy.visible,
    "Copy menu item is displayed as expected");

  try {
    await waitForClipboardPromise(() => menuitemCopy.click(),
      () => checkClipboardData(expectedPattern));
  } catch (e) {
    failedClipboard(expectedPattern);
  }
}

async function checkSelectAll(view) {
  info("Testing select-all copy");

  let contentDoc = view.styleDocument;
  let prop = contentDoc.querySelector(".ruleview-property");

  info("Checking that _SelectAll() then copy returns the correct " +
    "clipboard value");
  view.contextMenu._onSelectAll();
  let expectedPattern = "element {[\\r\\n]+" +
                        "    margin: 10em;[\\r\\n]+" +
                        "    font-size: 14pt;[\\r\\n]+" +
                        "    font-family: helvetica, sans-serif;[\\r\\n]+" +
                        "    color: #AAA;[\\r\\n]+" +
                        "}[\\r\\n]+" +
                        "html {[\\r\\n]+" +
                        "    color: #000000;[\\r\\n]+" +
                        "}[\\r\\n]*";

  let allMenuItems = openStyleContextMenuAndGetAllItems(view, prop);
  let menuitemCopy = allMenuItems.find(item => item.label ===
    STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copy"));

  ok(menuitemCopy.visible,
    "Copy menu item is displayed as expected");

  try {
    await waitForClipboardPromise(() => menuitemCopy.click(),
      () => checkClipboardData(expectedPattern));
  } catch (e) {
    failedClipboard(expectedPattern);
  }
}

async function checkCopyEditorValue(view) {
  info("Testing CSS property editor value copy");

  let ruleEditor = getRuleViewRuleEditor(view, 0);
  let propEditor = ruleEditor.rule.textProps[0].editor;

  let editor = await focusEditableField(view, propEditor.valueSpan);

  info("Checking that copying a css property value editor returns the correct" +
    " clipboard value");

  let expectedPattern = "10em";

  let allMenuItems = openStyleContextMenuAndGetAllItems(view, editor.input);
  let menuitemCopy = allMenuItems.find(item => item.label ===
    STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copy"));

  ok(menuitemCopy.visible,
    "Copy menu item is displayed as expected");

  try {
    await waitForClipboardPromise(() => menuitemCopy.click(),
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
