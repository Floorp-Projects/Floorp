/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that collapsibilities of DebugTargetPane on RuntimePage by mouse clicking.
 */

add_task(async function() {
  prepareCollapsibilitiesTest();

  const { document, tab } = await openAboutDebugging();

  for (const { title } of TARGET_PANES) {
    const debugTargetPaneEl = getDebugTargetPane(title, document);

    info("Check whether this pane is collapsed after clicking the title");
    await toggleCollapsibility(debugTargetPaneEl);
    assertCollapsibility(debugTargetPaneEl, title, true);

    info("Check whether this pane is expanded after clicking the title again");
    await toggleCollapsibility(debugTargetPaneEl);
    assertCollapsibility(debugTargetPaneEl, title, false);
  }

  await removeTab(tab);
});

function assertCollapsibility(debugTargetPaneEl, title, shouldCollapsed) {
  info("Check height of list");
  const listEl = debugTargetPaneEl.querySelector(".js-debug-target-list");
  const assertHeight = shouldCollapsed ? is : isnot;
  assertHeight(listEl.clientHeight, 0, "Height of list element should correct");

  info("Check content of title");
  const titleEl = debugTargetPaneEl.querySelector(".js-debug-target-pane-title");
  const expectedTitle =
    shouldCollapsed
      ? `${ title } (${ listEl.querySelectorAll(".js-debug-target-item").length })`
      : title;
  is(titleEl.textContent, expectedTitle, "Title should correct");
}
