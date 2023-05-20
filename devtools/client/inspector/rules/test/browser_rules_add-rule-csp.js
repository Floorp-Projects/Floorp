/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `
<!doctype html>
<html>
  <head>
    <meta http-equiv="Content-Security-Policy" content="style-src 'none'">
  </head>
  <body>
    <div id="testid"></div>
  </body>
</html>
`;

// Tests adding a new rule works on a page with CSP style-src none.
add_task(async function () {
  await addTab(`data:text/html;charset=utf-8,${encodeURIComponent(TEST_URI)}`);
  const { inspector, view } = await openRuleView();

  info("Selecting the test node");
  await selectNode("#testid", inspector);

  info("Adding a new rule for this node and blurring the new selector field");
  await addNewRuleAndDismissEditor(inspector, view, "#testid", 1);

  info("Adding a new property for this rule");
  const ruleEditor = getRuleViewRuleEditor(view, 1);

  const onRuleViewChanged = view.once("ruleview-changed");
  ruleEditor.addProperty("color", "red", "", true);
  await onRuleViewChanged;

  const textProps = ruleEditor.rule.textProps;
  const prop = textProps[textProps.length - 1];
  is(prop.name, "color", "The last property name is color");
  is(prop.value, "red", "The last property value is red");
});
