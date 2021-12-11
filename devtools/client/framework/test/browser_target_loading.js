/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that toolbox can be opened right after a tab is added, while the document
// is still loading.

add_task(async function testOpenToolboxOnLoadingDocument() {
  const TEST_URI =
    `https://example.com/document-builder.sjs?` +
    `html=Test<script>console.log("page loaded")</script>`;

  // ⚠️ Note that we don't await for `addTab` here, as we want to open the toolbox just
  // after the tab is addded, with the document still loading.
  info("Add tab…");
  const onTabAdded = addTab(TEST_URI);
  const tab = gBrowser.selectedTab;
  info("…and open the toolbox right away");
  const onToolboxShown = gDevTools.showToolboxForTab(tab, {
    toolId: "webconsole",
  });

  await onTabAdded;
  ok(true, "The tab as done loading");

  const toolbox = await onToolboxShown;
  ok(true, "The toolbox is shown");

  info("Check that the console opened and has the message from the page");
  const { hud } = toolbox.getPanel("webconsole");
  await waitFor(() =>
    Array.from(
      hud.ui.window.document.querySelectorAll(".message-body")
    ).some(el => el.innerText.includes("page loaded"))
  );
  ok(true, "The console opened with the expected content");
});
