/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests the behaviour of the copy styles context menu items in the rule
 * view.
 */

const osString = Services.appinfo.OS;

const TEST_URI = URL_ROOT_SSL + "doc_copystyles.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);

  const ruleEditor = getRuleViewRuleEditor(view, 1);

  const data = [
    {
      desc: "Test Copy Property Name",
      node: ruleEditor.rule.textProps[0].editor.nameSpan,
      menuItemLabel: "styleinspector.contextmenu.copyPropertyName",
      expectedPattern: "color",
      visible: {
        copyLocation: false,
        copyDeclaration: true,
        copyPropertyName: true,
        copyPropertyValue: false,
        copySelector: false,
        copyRule: true,
      },
    },
    {
      desc: "Test Copy Property Value",
      node: ruleEditor.rule.textProps[2].editor.valueSpan,
      menuItemLabel: "styleinspector.contextmenu.copyPropertyValue",
      expectedPattern: "12px",
      visible: {
        copyLocation: false,
        copyDeclaration: true,
        copyPropertyName: false,
        copyPropertyValue: true,
        copySelector: false,
        copyRule: true,
      },
    },
    {
      desc: "Test Copy Property Value with Priority",
      node: ruleEditor.rule.textProps[3].editor.valueSpan,
      menuItemLabel: "styleinspector.contextmenu.copyPropertyValue",
      expectedPattern: "#00F !important",
      visible: {
        copyLocation: false,
        copyDeclaration: true,
        copyPropertyName: false,
        copyPropertyValue: true,
        copySelector: false,
        copyRule: true,
      },
    },
    {
      desc: "Test Copy Property Declaration",
      node: ruleEditor.rule.textProps[2].editor.nameSpan,
      menuItemLabel: "styleinspector.contextmenu.copyDeclaration",
      expectedPattern: "font-size: 12px;",
      visible: {
        copyLocation: false,
        copyDeclaration: true,
        copyPropertyName: true,
        copyPropertyValue: false,
        copySelector: false,
        copyRule: true,
      },
    },
    {
      desc: "Test Copy Property Declaration with Priority",
      node: ruleEditor.rule.textProps[3].editor.nameSpan,
      menuItemLabel: "styleinspector.contextmenu.copyDeclaration",
      expectedPattern: "border-color: #00F !important;",
      visible: {
        copyLocation: false,
        copyDeclaration: true,
        copyPropertyName: true,
        copyPropertyValue: false,
        copySelector: false,
        copyRule: true,
      },
    },
    {
      desc: "Test Copy Rule",
      node: ruleEditor.rule.textProps[2].editor.nameSpan,
      menuItemLabel: "styleinspector.contextmenu.copyRule",
      expectedPattern:
        "#testid {[\\r\\n]+" +
        "\tcolor: #F00;[\\r\\n]+" +
        "\tbackground-color: #00F;[\\r\\n]+" +
        "\tfont-size: 12px;[\\r\\n]+" +
        "\tborder-color: #00F !important;[\\r\\n]+" +
        '\t--var: "\\*/";[\\r\\n]+' +
        "}",
      visible: {
        copyLocation: false,
        copyDeclaration: true,
        copyPropertyName: true,
        copyPropertyValue: false,
        copySelector: false,
        copyRule: true,
      },
    },
    {
      desc: "Test Copy Selector",
      node: ruleEditor.selectorText,
      menuItemLabel: "styleinspector.contextmenu.copySelector",
      expectedPattern: "html, body, #testid",
      visible: {
        copyLocation: false,
        copyDeclaration: false,
        copyPropertyName: false,
        copyPropertyValue: false,
        copySelector: true,
        copyRule: true,
      },
    },
    {
      desc: "Test Copy Location",
      node: ruleEditor.source,
      menuItemLabel: "styleinspector.contextmenu.copyLocation",
      expectedPattern:
        "https://example.com/browser/devtools/client/" +
        "inspector/rules/test/doc_copystyles.css",
      visible: {
        copyLocation: true,
        copyDeclaration: false,
        copyPropertyName: false,
        copyPropertyValue: false,
        copySelector: false,
        copyRule: true,
      },
    },
    {
      async setup() {
        await disableProperty(view, 0);
      },
      desc: "Test Copy Rule with Disabled Property",
      node: ruleEditor.rule.textProps[2].editor.nameSpan,
      menuItemLabel: "styleinspector.contextmenu.copyRule",
      expectedPattern:
        "#testid {[\\r\\n]+" +
        "\t/\\* color: #F00; \\*/[\\r\\n]+" +
        "\tbackground-color: #00F;[\\r\\n]+" +
        "\tfont-size: 12px;[\\r\\n]+" +
        "\tborder-color: #00F !important;[\\r\\n]+" +
        '\t--var: "\\*/";[\\r\\n]+' +
        "}",
      visible: {
        copyLocation: false,
        copyDeclaration: true,
        copyPropertyName: true,
        copyPropertyValue: false,
        copySelector: false,
        copyRule: true,
      },
    },
    {
      async setup() {
        await disableProperty(view, 4);
      },
      desc: "Test Copy Rule with Disabled Property with Comment",
      node: ruleEditor.rule.textProps[2].editor.nameSpan,
      menuItemLabel: "styleinspector.contextmenu.copyRule",
      expectedPattern:
        "#testid {[\\r\\n]+" +
        "\t/\\* color: #F00; \\*/[\\r\\n]+" +
        "\tbackground-color: #00F;[\\r\\n]+" +
        "\tfont-size: 12px;[\\r\\n]+" +
        "\tborder-color: #00F !important;[\\r\\n]+" +
        '\t/\\* --var: "\\*\\\\/"; \\*/[\\r\\n]+' +
        "}",
      visible: {
        copyLocation: false,
        copyDeclaration: true,
        copyPropertyName: true,
        copyPropertyValue: false,
        copySelector: false,
        copyRule: true,
      },
    },
    {
      desc: "Test Copy Property Declaration with Disabled Property",
      node: ruleEditor.rule.textProps[0].editor.nameSpan,
      menuItemLabel: "styleinspector.contextmenu.copyDeclaration",
      expectedPattern: "/\\* color: #F00; \\*/",
      visible: {
        copyLocation: false,
        copyDeclaration: true,
        copyPropertyName: true,
        copyPropertyValue: false,
        copySelector: false,
        copyRule: true,
      },
    },
  ];

  for (const {
    setup,
    desc,
    node,
    menuItemLabel,
    expectedPattern,
    visible,
  } of data) {
    if (setup) {
      await setup();
    }

    info(desc);
    await checkCopyStyle(view, node, menuItemLabel, expectedPattern, visible);
  }
});

async function checkCopyStyle(
  view,
  node,
  menuItemLabel,
  expectedPattern,
  visible
) {
  const allMenuItems = openStyleContextMenuAndGetAllItems(view, node);
  const menuItem = allMenuItems.find(
    item => item.label === STYLE_INSPECTOR_L10N.getStr(menuItemLabel)
  );
  const menuitemCopy = allMenuItems.find(
    item =>
      item.label ===
      STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copy")
  );
  const menuitemCopyLocation = allMenuItems.find(
    item =>
      item.label ===
      STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copyLocation")
  );
  const menuitemCopyDeclaration = allMenuItems.find(
    item =>
      item.label ===
      STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copyDeclaration")
  );
  const menuitemCopyPropertyName = allMenuItems.find(
    item =>
      item.label ===
      STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copyPropertyName")
  );
  const menuitemCopyPropertyValue = allMenuItems.find(
    item =>
      item.label ===
      STYLE_INSPECTOR_L10N.getStr(
        "styleinspector.contextmenu.copyPropertyValue"
      )
  );
  const menuitemCopySelector = allMenuItems.find(
    item =>
      item.label ===
      STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copySelector")
  );
  const menuitemCopyRule = allMenuItems.find(
    item =>
      item.label ===
      STYLE_INSPECTOR_L10N.getStr("styleinspector.contextmenu.copyRule")
  );

  ok(menuitemCopy.disabled, "Copy disabled is as expected: true");
  ok(menuitemCopy.visible, "Copy visible is as expected: true");

  is(
    menuitemCopyLocation.visible,
    visible.copyLocation,
    "Copy Location visible attribute is as expected: " + visible.copyLocation
  );

  is(
    menuitemCopyDeclaration.visible,
    visible.copyDeclaration,
    "Copy Property Declaration visible attribute is as expected: " +
      visible.copyDeclaration
  );

  is(
    menuitemCopyPropertyName.visible,
    visible.copyPropertyName,
    "Copy Property Name visible attribute is as expected: " +
      visible.copyPropertyName
  );

  is(
    menuitemCopyPropertyValue.visible,
    visible.copyPropertyValue,
    "Copy Property Value visible attribute is as expected: " +
      visible.copyPropertyValue
  );

  is(
    menuitemCopySelector.visible,
    visible.copySelector,
    "Copy Selector visible attribute is as expected: " + visible.copySelector
  );

  is(
    menuitemCopyRule.visible,
    visible.copyRule,
    "Copy Rule visible attribute is as expected: " + visible.copyRule
  );

  try {
    await waitForClipboardPromise(
      () => menuItem.click(),
      () => checkClipboardData(expectedPattern)
    );
  } catch (e) {
    failedClipboard(expectedPattern);
  }
}

async function disableProperty(view, index) {
  const ruleEditor = getRuleViewRuleEditor(view, 1);
  const textProp = ruleEditor.rule.textProps[index];
  await togglePropStatus(view, textProp);
}

function checkClipboardData(expectedPattern) {
  const actual = SpecialPowers.getClipboardData("text/unicode");
  const expectedRegExp = new RegExp(expectedPattern, "g");
  return expectedRegExp.test(actual);
}

function failedClipboard(expectedPattern) {
  // Format expected text for comparison
  const terminator = osString == "WINNT" ? "\r\n" : "\n";
  expectedPattern = expectedPattern.replace(/\[\\r\\n\][+*]/g, terminator);
  expectedPattern = expectedPattern.replace(/\\\(/g, "(");
  expectedPattern = expectedPattern.replace(/\\\)/g, ")");

  let actual = SpecialPowers.getClipboardData("text/unicode");

  // Trim the right hand side of our strings. This is because expectedPattern
  // accounts for windows sometimes adding a newline to our copied data.
  expectedPattern = expectedPattern.trimRight();
  actual = actual.trimRight();

  ok(
    false,
    "Clipboard text does not match expected " +
      "results (escaped for accurate comparison):\n"
  );
  info("Actual: " + escape(actual));
  info("Expected: " + escape(expectedPattern));
}
