/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the behaviour of the copy styles context menu items in the rule
 * view.
 */

const osString = Services.appinfo.OS;

const TEST_URI = URL_ROOT + "doc_copystyles.html";

add_task(function* () {
  yield addTab(TEST_URI);
  let { inspector, view } = yield openRuleView();
  yield selectNode("#testid", inspector);

  let ruleEditor = getRuleViewRuleEditor(view, 1);

  let data = [
    {
      desc: "Test Copy Property Name",
      node: ruleEditor.rule.textProps[0].editor.nameSpan,
      menuItemLabel: "styleinspector.contextmenu.copyPropertyName",
      expectedPattern: "color",
      visible: {
        copyLocation: false,
        copyPropertyDeclaration: true,
        copyPropertyName: true,
        copyPropertyValue: false,
        copySelector: false,
        copyRule: true
      }
    },
    {
      desc: "Test Copy Property Value",
      node: ruleEditor.rule.textProps[2].editor.valueSpan,
      menuItemLabel: "styleinspector.contextmenu.copyPropertyValue",
      expectedPattern: "12px",
      visible: {
        copyLocation: false,
        copyPropertyDeclaration: true,
        copyPropertyName: false,
        copyPropertyValue: true,
        copySelector: false,
        copyRule: true
      }
    },
    {
      desc: "Test Copy Property Value with Priority",
      node: ruleEditor.rule.textProps[3].editor.valueSpan,
      menuItemLabel: "styleinspector.contextmenu.copyPropertyValue",
      expectedPattern: "#00F !important",
      visible: {
        copyLocation: false,
        copyPropertyDeclaration: true,
        copyPropertyName: false,
        copyPropertyValue: true,
        copySelector: false,
        copyRule: true
      }
    },
    {
      desc: "Test Copy Property Declaration",
      node: ruleEditor.rule.textProps[2].editor.nameSpan,
      menuItemLabel: "styleinspector.contextmenu.copyPropertyDeclaration",
      expectedPattern: "font-size: 12px;",
      visible: {
        copyLocation: false,
        copyPropertyDeclaration: true,
        copyPropertyName: true,
        copyPropertyValue: false,
        copySelector: false,
        copyRule: true
      }
    },
    {
      desc: "Test Copy Property Declaration with Priority",
      node: ruleEditor.rule.textProps[3].editor.nameSpan,
      menuItemLabel: "styleinspector.contextmenu.copyPropertyDeclaration",
      expectedPattern: "border-color: #00F !important;",
      visible: {
        copyLocation: false,
        copyPropertyDeclaration: true,
        copyPropertyName: true,
        copyPropertyValue: false,
        copySelector: false,
        copyRule: true
      }
    },
    {
      desc: "Test Copy Rule",
      node: ruleEditor.rule.textProps[2].editor.nameSpan,
      menuItemLabel: "styleinspector.contextmenu.copyRule",
      expectedPattern: "#testid {[\\r\\n]+" +
                       "\tcolor: #F00;[\\r\\n]+" +
                       "\tbackground-color: #00F;[\\r\\n]+" +
                       "\tfont-size: 12px;[\\r\\n]+" +
                       "\tborder-color: #00F !important;[\\r\\n]+" +
                       "\t--var: \"\\*/\";[\\r\\n]+" +
                       "}",
      visible: {
        copyLocation: false,
        copyPropertyDeclaration: true,
        copyPropertyName: true,
        copyPropertyValue: false,
        copySelector: false,
        copyRule: true
      }
    },
    {
      desc: "Test Copy Selector",
      node: ruleEditor.selectorText,
      menuItemLabel: "styleinspector.contextmenu.copySelector",
      expectedPattern: "html, body, #testid",
      visible: {
        copyLocation: false,
        copyPropertyDeclaration: false,
        copyPropertyName: false,
        copyPropertyValue: false,
        copySelector: true,
        copyRule: true
      }
    },
    {
      desc: "Test Copy Location",
      node: ruleEditor.source,
      menuItemLabel: "styleinspector.contextmenu.copyLocation",
      expectedPattern: "http://example.com/browser/devtools/client/" +
                       "inspector/rules/test/doc_copystyles.css",
      visible: {
        copyLocation: true,
        copyPropertyDeclaration: false,
        copyPropertyName: false,
        copyPropertyValue: false,
        copySelector: false,
        copyRule: true
      }
    },
    {
      setup: function* () {
        yield disableProperty(view, 0);
      },
      desc: "Test Copy Rule with Disabled Property",
      node: ruleEditor.rule.textProps[2].editor.nameSpan,
      menuItemLabel: "styleinspector.contextmenu.copyRule",
      expectedPattern: "#testid {[\\r\\n]+" +
                       "\t\/\\* color: #F00; \\*\/[\\r\\n]+" +
                       "\tbackground-color: #00F;[\\r\\n]+" +
                       "\tfont-size: 12px;[\\r\\n]+" +
                       "\tborder-color: #00F !important;[\\r\\n]+" +
                       "\t--var: \"\\*/\";[\\r\\n]+" +
                       "}",
      visible: {
        copyLocation: false,
        copyPropertyDeclaration: true,
        copyPropertyName: true,
        copyPropertyValue: false,
        copySelector: false,
        copyRule: true
      }
    },
    {
      setup: function* () {
        yield disableProperty(view, 4);
      },
      desc: "Test Copy Rule with Disabled Property with Comment",
      node: ruleEditor.rule.textProps[2].editor.nameSpan,
      menuItemLabel: "styleinspector.contextmenu.copyRule",
      expectedPattern: "#testid {[\\r\\n]+" +
                       "\t\/\\* color: #F00; \\*\/[\\r\\n]+" +
                       "\tbackground-color: #00F;[\\r\\n]+" +
                       "\tfont-size: 12px;[\\r\\n]+" +
                       "\tborder-color: #00F !important;[\\r\\n]+" +
                       "\t/\\* --var: \"\\*\\\\\/\"; \\*\/[\\r\\n]+" +
                       "}",
      visible: {
        copyLocation: false,
        copyPropertyDeclaration: true,
        copyPropertyName: true,
        copyPropertyValue: false,
        copySelector: false,
        copyRule: true
      }
    },
    {
      desc: "Test Copy Property Declaration with Disabled Property",
      node: ruleEditor.rule.textProps[0].editor.nameSpan,
      menuItemLabel: "styleinspector.contextmenu.copyPropertyDeclaration",
      expectedPattern: "\/\\* color: #F00; \\*\/",
      visible: {
        copyLocation: false,
        copyPropertyDeclaration: true,
        copyPropertyName: true,
        copyPropertyValue: false,
        copySelector: false,
        copyRule: true
      }
    },
  ];

  for (let { setup, desc, node, menuItemLabel, expectedPattern, visible } of data) {
    if (setup) {
      yield setup();
    }

    info(desc);
    yield checkCopyStyle(view, node, menuItemLabel, expectedPattern, visible);
  }
});

function* checkCopyStyle(view, node, menuItemLabel, expectedPattern, visible) {
  let allMenuItems = openStyleContextMenuAndGetAllItems(view, node);
  let menuItem = allMenuItems.find(item =>
    item.label === _STRINGS.GetStringFromName(menuItemLabel));
  let menuitemCopy = allMenuItems.find(item => item.label ===
    _STRINGS.GetStringFromName("styleinspector.contextmenu.copy"));
  let menuitemCopyLocation = allMenuItems.find(item => item.label ===
    _STRINGS.GetStringFromName("styleinspector.contextmenu.copyLocation"));
  let menuitemCopyPropertyDeclaration = allMenuItems.find(item => item.label ===
    _STRINGS.GetStringFromName("styleinspector.contextmenu.copyPropertyDeclaration"));
  let menuitemCopyPropertyName = allMenuItems.find(item => item.label ===
    _STRINGS.GetStringFromName("styleinspector.contextmenu.copyPropertyName"));
  let menuitemCopyPropertyValue = allMenuItems.find(item => item.label ===
    _STRINGS.GetStringFromName("styleinspector.contextmenu.copyPropertyValue"));
  let menuitemCopySelector = allMenuItems.find(item => item.label ===
    _STRINGS.GetStringFromName("styleinspector.contextmenu.copySelector"));
  let menuitemCopyRule = allMenuItems.find(item => item.label ===
    _STRINGS.GetStringFromName("styleinspector.contextmenu.copyRule"));

  ok(menuitemCopy.disabled,
    "Copy disabled is as expected: true");
  ok(menuitemCopy.visible,
    "Copy visible is as expected: true");

  is(menuitemCopyLocation.visible,
     visible.copyLocation,
     "Copy Location visible attribute is as expected: " +
     visible.copyLocation);

  is(menuitemCopyPropertyDeclaration.visible,
     visible.copyPropertyDeclaration,
     "Copy Property Declaration visible attribute is as expected: " +
     visible.copyPropertyDeclaration);

  is(menuitemCopyPropertyName.visible,
     visible.copyPropertyName,
     "Copy Property Name visible attribute is as expected: " +
     visible.copyPropertyName);

  is(menuitemCopyPropertyValue.visible,
     visible.copyPropertyValue,
     "Copy Property Value visible attribute is as expected: " +
     visible.copyPropertyValue);

  is(menuitemCopySelector.visible,
     visible.copySelector,
     "Copy Selector visible attribute is as expected: " +
     visible.copySelector);

  is(menuitemCopyRule.visible,
     visible.copyRule,
     "Copy Rule visible attribute is as expected: " +
     visible.copyRule);

  try {
    yield waitForClipboard(() => menuItem.click(),
      () => checkClipboardData(expectedPattern));
  } catch (e) {
    failedClipboard(expectedPattern);
  }
}

function* disableProperty(view, index) {
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let textProp = ruleEditor.rule.textProps[index];
  yield togglePropStatus(view, textProp);
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
