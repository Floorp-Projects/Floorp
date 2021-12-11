/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule view does not go blank while selecting a new node.

const TESTCASE_URI =
  "data:text/html;charset=utf-8," +
  '<div id="testdiv" style="font-size:10px;">' +
  "Test div!</div>";

add_task(async function() {
  await addTab(TESTCASE_URI);

  info("Opening the rule view and selecting the test node");
  const { inspector, view } = await openRuleView();
  const testdiv = await getNodeFront("#testdiv", inspector);
  await selectNode(testdiv, inspector);

  const htmlBefore = view.element.innerHTML;
  ok(
    htmlBefore.indexOf("font-size") > -1,
    "The rule view should contain a font-size property."
  );

  // Do the selectNode call manually, because otherwise it's hard to guarantee
  // that we can make the below checks at a reasonable time.
  info("refreshing the node");
  const p = view.selectElement(testdiv, true);
  is(
    view.element.innerHTML,
    htmlBefore,
    "The rule view is unchanged during selection."
  );
  ok(
    view.element.classList.contains("non-interactive"),
    "The rule view is marked non-interactive."
  );
  await p;

  info("node refreshed");
  ok(
    !view.element.classList.contains("non-interactive"),
    "The rule view is marked interactive again."
  );
});
