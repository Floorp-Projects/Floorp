/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that clicking on a property's value URL while editing the property name
// will open the link in a new tab. See also Bug 1248274.

const TEST_URI = `
  <style type="text/css">
  #testid {
      background: url("http://example.com/browser/devtools/client/inspector/rules/test/doc_test_image.png"), linear-gradient(white, #F06 400px);
  }
  </style>
  <div id="testid">Styled Node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = await openRuleView();

  info("Test click on background-image url while editing property name");

  await selectNode("#testid", inspector);
  let ruleEditor = getRuleViewRuleEditor(view, 1);
  let propEditor = ruleEditor.rule.textProps[0].editor;
  let anchor = propEditor.valueSpan.querySelector(".ruleview-propertyvalue .theme-link");

  info("Focus the background name span");
  await focusEditableField(view, propEditor.nameSpan);
  let editor = inplaceEditor(propEditor.doc.activeElement);

  info("Modify the property to background to trigger the " +
    "property-value-updated event");
  editor.input.value = "background-image";

  let onRuleViewChanged = view.once("ruleview-changed");
  let onPropertyValueUpdate = view.once("property-value-updated");
  let onTabOpened = waitForTab();

  info("blur propEditor.nameSpan by clicking on the link");
  // The url can be wrapped across multiple lines, and so we click the lower left corner
  // of the anchor to make sure to target the link.
  let rect = anchor.getBoundingClientRect();
  EventUtils.synthesizeMouse(anchor, 2, rect.height - 2, {}, propEditor.doc.defaultView);

  info("wait for ruleview-changed event to be triggered to prevent pending requests");
  await onRuleViewChanged;

  info("wait for the property value to be updated");
  await onPropertyValueUpdate;

  info("wait for the image to be open in a new tab");
  let tab = await onTabOpened;
  ok(true, "A new tab opened");

  is(tab.linkedBrowser.currentURI.spec, anchor.href,
    "The URL for the new tab is correct");

  gBrowser.removeTab(tab);
});
