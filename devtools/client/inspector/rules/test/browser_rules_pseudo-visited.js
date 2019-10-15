/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests for visited/unvisited rule.

const TEST_URI = URL_ROOT + "doc_visited.html";

add_task(async () => {
  info("Open a url which has a visited and an unvisited link");
  const tab = await addTab(TEST_URI);

  info("Wait until the visited link is available");
  await waitUntilVisitedState(tab, ["#visited"]);

  info("Open the inspector");
  const { inspector, view } = await openRuleView();

  info("Check whether the rule view is shown correctly for visited element");
  await selectNode("#visited", inspector);
  ok(getRuleViewRule(view, "a:visited"), "Rule of a:visited is shown");
  ok(!getRuleViewRule(view, "a:link"), "Rule of a:link is not shown");
  ok(getRuleViewRule(view, "a"), "Rule of a is shown");

  info("Check whether the rule view is shown correctly for unvisited element");
  await selectNode("#unvisited", inspector);
  ok(!getRuleViewRule(view, "a:visited"), "Rule of a:visited is not shown");
  ok(getRuleViewRule(view, "a:link"), "Rule of a:link is shown");
  ok(getRuleViewRule(view, "a"), "Rule of a is shown");
});
