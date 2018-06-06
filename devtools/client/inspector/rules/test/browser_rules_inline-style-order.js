/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that when the order of properties in the inline style changes, the inline style
// rule updates accordingly.
// This can happen in cases such as this one:
// Given this DOM node:
// <div style="margin:0;color:red;"></div>
// Executing this:
// element.style.margin = "10px";
// Will result in the following attribute value:
// <div style="color: red; margin: 10px;"></div>
// The inline style rule in the rule-view need to update to reflect the new order of
// properties accordingly.
// Note however that we do not want to expect a specific order in this test, and be
// subject to failures if it changes again. Instead, the test compares the order against
// what is in the style DOM attribute.
// See bug 1467076.

// Test cases, these are { name, value } objects used to change the DOM element's style
// property. After each of these changes, the inline style rule's content will be checked
// against the style DOM attribute's content.
const TEST_CASES = [
  { name: "margin", value: "10px" },
  { name: "color", value: "blue" },
  { name: "padding", value: "20px" },
  { name: "margin", value: "0px" },
  { name: "color", value: "black" },
];

add_task(async function() {
  const { linkedBrowser: browser } = await addTab(
    `data:text/html;charset=utf-8,<div style="margin:0;color:red;">Inspect me!</div>`);

  const { inspector, view } = await openRuleView();
  await selectNode("div", inspector);

  for (const { name, value } of TEST_CASES) {
    info(`Setting style.${name} to ${value} on the test node`);

    const onStyleMutation = waitForStyleModification(inspector);
    const onRuleRefreshed = inspector.once("rule-view-refreshed");
    await ContentTask.spawn(browser, { name, value }, async function(change) {
      content.document.querySelector("div").style[change.name] = change.value;
    });
    await Promise.all([onStyleMutation, onRuleRefreshed]);

    info("Getting and parsing the content of the node's style attribute");
    const markupContainer = inspector.markup.getContainer(inspector.selection.nodeFront);
    const styleAttrValue = markupContainer.elt.querySelector(".attr-value").textContent;
    const parsedStyleAttr =
      styleAttrValue.split(";").filter(v => v.trim())
                    .map(decl => {
                      const nameValue = decl.split(":").map(v => v.trim());
                      return { name: nameValue[0], value: nameValue[1] };
                    });

    info("Checking the content of the rule-view");
    const ruleEditor = getRuleViewRuleEditor(view, 0);
    const propertiesEls = ruleEditor.propertyList.children;

    parsedStyleAttr.forEach((expected, i) => {
      is(propertiesEls[i].querySelector(".ruleview-propertyname").textContent,
         expected.name, `Correct name found for property ${i}`);
      is(propertiesEls[i].querySelector(".ruleview-propertyvalue").textContent,
         expected.value, `Correct value found for property ${i}`);
    });
  }
});
