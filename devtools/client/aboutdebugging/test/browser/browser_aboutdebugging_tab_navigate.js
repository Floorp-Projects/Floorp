/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TAB_1_URL =
  "http://example.org/document-builder.sjs?html=<title>TITLE1</title>";
const TAB_2_URL =
  "http://example.org/document-builder.sjs?html=<title>TITLE2</title>";

// Check that the list of tabs in about:debugging is updated when a page
// navigates. This indirectly checks that the tabListChanged event is correctly
// fired from the root actor.
add_task(async function () {
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  const testTab = await addTab(TAB_1_URL, { background: true });
  await waitFor(() => findDebugTargetByText("TITLE1", document));

  navigateTo(TAB_2_URL, { browser: testTab.linkedBrowser });
  await waitFor(() => findDebugTargetByText("TITLE2", document));

  ok(
    !findDebugTargetByText("TITLE1", document),
    "TITLE2 target replaced TITLE1"
  );

  await removeTab(tab);
  await removeTab(testTab);
});
