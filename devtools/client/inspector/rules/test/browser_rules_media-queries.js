/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that we correctly display appropriate media query titles in the
// rule view.

const TEST_URI = URL_ROOT + "doc_media_queries.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();
  await selectNode("div", inspector);

  const elementStyle = view._elementStyle;

  const inline = STYLE_INSPECTOR_L10N.getStr("rule.sourceInline");

  is(elementStyle.rules.length, 3, "Should have 3 rules.");
  is(elementStyle.rules[0].title, inline, "check rule 0 title");
  is(
    elementStyle.rules[1].title,
    inline + ":9 @media screen and (min-width: 1px)",
    "check rule 1 title"
  );
  is(elementStyle.rules[2].title, inline + ":2", "check rule 2 title");
});
