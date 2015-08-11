/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the behaviour of the copy styles context menu items in the rule
 * view.
 */

XPCOMUtils.defineLazyGetter(this, "osString", function() {
  return Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS;
});

const TEST_URI = TEST_URL_ROOT + "doc_copystyles.html";

add_task(function*() {
  yield addTab(TEST_URI);
  let { inspector, view } = yield openRuleView();
  let contextmenu = view._contextmenu;
  yield selectNode("#testid", inspector);

  let ruleEditor = getRuleViewRuleEditor(view, 1);

  let data = [
    {
      desc: "Test Copy Property Name",
      node: ruleEditor.rule.textProps[0].editor.nameSpan,
      menuItem: contextmenu.menuitemCopyPropertyName,
      expectedPattern: "color",
      hidden: {
        copyLocation: true,
        copyPropertyDeclaration: false,
        copyPropertyName: false,
        copyPropertyValue: true,
        copySelector: true,
        copyRule: false
      }
    },
    {
      desc: "Test Copy Property Value",
      node: ruleEditor.rule.textProps[2].editor.valueSpan,
      menuItem: contextmenu.menuitemCopyPropertyValue,
      expectedPattern: "12px",
      hidden: {
        copyLocation: true,
        copyPropertyDeclaration: false,
        copyPropertyName: true,
        copyPropertyValue: false,
        copySelector: true,
        copyRule: false
      }
    },
    {
      desc: "Test Copy Property Value with Priority",
      node: ruleEditor.rule.textProps[3].editor.valueSpan,
      menuItem: contextmenu.menuitemCopyPropertyValue,
      expectedPattern: "#00F !important",
      hidden: {
        copyLocation: true,
        copyPropertyDeclaration: false,
        copyPropertyName: true,
        copyPropertyValue: false,
        copySelector: true,
        copyRule: false
      }
    },
    {
      desc: "Test Copy Property Declaration",
      node: ruleEditor.rule.textProps[2].editor.nameSpan,
      menuItem: contextmenu.menuitemCopyPropertyDeclaration,
      expectedPattern: "font-size: 12px;",
      hidden: {
        copyLocation: true,
        copyPropertyDeclaration: false,
        copyPropertyName: false,
        copyPropertyValue: true,
        copySelector: true,
        copyRule: false
      }
    },
    {
      desc: "Test Copy Property Declaration with Priority",
      node: ruleEditor.rule.textProps[3].editor.nameSpan,
      menuItem: contextmenu.menuitemCopyPropertyDeclaration,
      expectedPattern: "border-color: #00F !important;",
      hidden: {
        copyLocation: true,
        copyPropertyDeclaration: false,
        copyPropertyName: false,
        copyPropertyValue: true,
        copySelector: true,
        copyRule: false
      }
    },
    {
      desc: "Test Copy Rule",
      node: ruleEditor.rule.textProps[2].editor.nameSpan,
      menuItem: contextmenu.menuitemCopyRule,
      expectedPattern: "#testid {[\\r\\n]+" +
                       "\tcolor: #F00;[\\r\\n]+" +
                       "\tbackground-color: #00F;[\\r\\n]+" +
                       "\tfont-size: 12px;[\\r\\n]+" +
                       "\tborder-color: #00F !important;[\\r\\n]+" +
                       "}",
      hidden: {
        copyLocation: true,
        copyPropertyDeclaration: false,
        copyPropertyName: false,
        copyPropertyValue: true,
        copySelector: true,
        copyRule: false
      }
    },
    {
      desc: "Test Copy Selector",
      node: ruleEditor.selectorText,
      menuItem: contextmenu.menuitemCopySelector,
      expectedPattern: "html, body, #testid",
      hidden: {
        copyLocation: true,
        copyPropertyDeclaration: true,
        copyPropertyName: true,
        copyPropertyValue: true,
        copySelector: false,
        copyRule: false
      }
    },
    {
      desc: "Test Copy Location",
      node: ruleEditor.source,
      menuItem: contextmenu.menuitemCopyLocation,
      expectedPattern: "http://example.com/browser/browser/devtools/" +
                       "styleinspector/test/doc_copystyles.css",
      hidden: {
        copyLocation: false,
        copyPropertyDeclaration: true,
        copyPropertyName: true,
        copyPropertyValue: true,
        copySelector: true,
        copyRule: false
      }
    },
    {
      setup: function*() {
        yield disableProperty(view);
      },
      desc: "Test Copy Rule with Disabled Property",
      node: ruleEditor.rule.textProps[2].editor.nameSpan,
      menuItem: contextmenu.menuitemCopyRule,
      expectedPattern: "#testid {[\\r\\n]+" +
                       "\t\/\\* color: #F00; \\*\/[\\r\\n]+" +
                       "\tbackground-color: #00F;[\\r\\n]+" +
                       "\tfont-size: 12px;[\\r\\n]+" +
                       "\tborder-color: #00F !important;[\\r\\n]+" +
                       "}",
      hidden: {
        copyLocation: true,
        copyPropertyDeclaration: false,
        copyPropertyName: false,
        copyPropertyValue: true,
        copySelector: true,
        copyRule: false
      }
    },
    {
      desc: "Test Copy Property Declaration with Disabled Property",
      node: ruleEditor.rule.textProps[0].editor.nameSpan,
      menuItem: contextmenu.menuitemCopyPropertyDeclaration,
      expectedPattern: "\/\\* color: #F00; \\*\/",
      hidden: {
        copyLocation: true,
        copyPropertyDeclaration: false,
        copyPropertyName: false,
        copyPropertyValue: true,
        copySelector: true,
        copyRule: false
      }
    },
  ];

  for (let { setup, desc, node, menuItem, expectedPattern, hidden } of data) {
    if (setup) {
      yield setup();
    }

    info(desc);
    yield checkCopyStyle(view, node, menuItem, expectedPattern, hidden);
  }
});

function* checkCopyStyle(view, node, menuItem, expectedPattern, hidden) {
  let onPopup = once(view._contextmenu._menupopup, "popupshown");
  EventUtils.synthesizeMouseAtCenter(node,
    {button: 2, type: "contextmenu"}, view.styleWindow);
  yield onPopup;

  ok(view._contextmenu.menuitemCopy.disabled,
    "Copy disabled is as expected: true");
  ok(!view._contextmenu.menuitemCopy.hidden,
    "Copy hidden is as expected: false");

  is(view._contextmenu.menuitemCopyLocation.hidden,
     hidden.copyLocation,
     "Copy Location hidden attribute is as expected: " +
     hidden.copyLocation);

  is(view._contextmenu.menuitemCopyPropertyDeclaration.hidden,
     hidden.copyPropertyDeclaration,
     "Copy Property Declaration hidden attribute is as expected: " +
     hidden.copyPropertyDeclaration);

  is(view._contextmenu.menuitemCopyPropertyName.hidden,
     hidden.copyPropertyName,
     "Copy Property Name hidden attribute is as expected: " +
     hidden.copyPropertyName);

  is(view._contextmenu.menuitemCopyPropertyValue.hidden,
     hidden.copyPropertyValue,
     "Copy Property Value hidden attribute is as expected: " +
     hidden.copyPropertyValue);

  is(view._contextmenu.menuitemCopySelector.hidden,
     hidden.copySelector,
     "Copy Selector hidden attribute is as expected: " +
     hidden.copySelector);

  is(view._contextmenu.menuitemCopyRule.hidden,
     hidden.copyRule,
     "Copy Rule hidden attribute is as expected: " +
     hidden.copyRule);

  try {
    yield waitForClipboard(() => menuItem.click(),
      () => checkClipboardData(expectedPattern));
  } catch(e) {
    failedClipboard(expectedPattern);
  }

  view._contextmenu._menupopup.hidePopup();
}

function* disableProperty(view) {
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let propEditor = ruleEditor.rule.textProps[0].editor;

  info("Disabling a property");
  propEditor.enable.click();
  yield ruleEditor.rule._applyingModifications;
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

  ok(false, "Clipboard text does not match expected " +
    "results (escaped for accurate comparison):\n");
  info("Actual: " + escape(actual));
  info("Expected: " + escape(expectedPattern));
}
