/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that StyleRuleActor.getRuleText returns the contents of the CSS rule.

const CSS_RULE = `#test {
  background-color: #f06;
}`;

const CONTENT = `
  <style type='text/css'>
    ${CSS_RULE}
  </style>
  <div id="test"></div>
`;

const TEST_URI = `data:text/html;charset=utf-8,${encodeURIComponent(CONTENT)}`;

add_task(async function () {
  const { inspector, target, walker } = await initInspectorFront(TEST_URI);

  const pageStyle = await inspector.getPageStyle();
  const element = await walker.querySelector(walker.rootNode, "#test");
  const entries = await pageStyle.getApplied(element, { inherited: false });

  const rule = entries[1].rule;
  const text = await rule.getRuleText();

  is(text, CSS_RULE, "CSS rule text content matches");

  await target.destroy();
});
