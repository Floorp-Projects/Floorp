/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that we correctly display appropriate media query information in the rule view.

const TEST_URI = URL_ROOT + "doc_media_queries.html?constructed";

add_task(async function () {
  await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();
  await selectNode("div", inspector);

  const elementStyle = view._elementStyle;

  const inline = STYLE_INSPECTOR_L10N.getStr("rule.sourceInline");
  const constructed = STYLE_INSPECTOR_L10N.getStr("rule.sourceConstructed");

  is(elementStyle.rules.length, 4, "Should have 4 rules.");
  is(elementStyle.rules[0].title, inline, "check rule 0 title");
  is(
    elementStyle.rules[1].title,
    constructed + ":1",
    "check constructed sheet rule title"
  );
  is(elementStyle.rules[2].title, inline + ":9", "check rule 2 title");
  is(elementStyle.rules[3].title, inline + ":2", "check rule 3 title");

  is(
    getRuleViewAncestorRulesDataTextByIndex(view, 2),
    "@media screen and (min-width: 1px) {",
    "Media queries information are displayed"
  );
});
