/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests for visited/unvisited rule.

const VISISTED_URI = URL_ROOT + "doc_variables_1.html";

const TEST_URI = `
  <style type='text/css'>
    a:visited { color: lime; }
    a:link { color: blue; }
    a { color: pink; }
  </style>
  <a href="${VISISTED_URI}" id="visited">visited link</a>
  <a href="#" id="unvisited">unvisited link</a>
`;

add_task(async () => {
  info("Open a particular url to make a visited link");
  const tab = await addTab(VISISTED_URI);

  info("Open tested page in the same tab");
  await BrowserTestUtils.loadURI(
    tab.linkedBrowser,
    "data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI)
  );

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
