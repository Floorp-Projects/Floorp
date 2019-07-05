/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view marks overridden rules correctly in pseudo-elements when
// selecting their host node.

const TEST_URI = `
  <style type='text/css'>
  #testid::before {
    content: 'Pseudo-element';
    color: red;
    color: green;
  }
  #testid {
    color: blue;
  }
  </style>
  <div id='testid'>Styled Node</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);

  info(
    "Check the CSS declarations for ::before in the Pseudo-elements accordion."
  );
  const pseudoRule = getRuleViewRuleEditor(view, 1, 0).rule;
  const pseudoProp1 = pseudoRule.textProps[1];
  const pseudoProp2 = pseudoRule.textProps[2];
  ok(
    pseudoProp1.overridden,
    "First declaration of color in pseudo-element should be overridden."
  );
  ok(
    !pseudoProp2.overridden,
    "Second declaration of color in pseudo-element should not be overridden."
  );

  info(
    "Check that pseudo-element declarations do not override the host's declarations"
  );
  const idRule = getRuleViewRuleEditor(view, 4).rule;
  const idProp = idRule.textProps[0];
  ok(
    !idProp.overridden,
    "The single declaration of color in ID selector should not be overridden"
  );
});
