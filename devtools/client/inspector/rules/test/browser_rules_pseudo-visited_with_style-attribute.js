/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view works for an element with a visited pseudo class
// with a style attribute defined.

const TEST_URI = URL_ROOT + "doc_visited_with_style_attribute.html";

add_task(async () => {
  info(
    "Open a page which has an element with a visited pseudo class and a style attribute"
  );
  const tab = await addTab(TEST_URI);

  info("Wait until the link has been visited");
  await waitUntilVisitedState(tab, ["#visited"]);

  info("Open the inspector");
  const { inspector, view } = await openRuleView();

  info("Check whether the rule view is shown correctly for visited element");
  await selectNode("#visited", inspector);
  ok(getRuleViewRule(view, "element"), "Rule of a is shown");
});
