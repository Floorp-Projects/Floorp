/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test switching for the top-level target.

const PARENT_PROCESS_URI = "about:robots";
const CONTENT_PROCESS_URI = TEST_BASE_HTTPS + "simple.html";

add_task(async function () {
  // We use about:robots, because this page will run in the parent process.
  // Navigating from about:robots to a regular content page will always trigger a target
  // switch, with or without fission.

  info("Open a page that runs in the parent process");
  const { ui } = await openStyleEditorForURL(PARENT_PROCESS_URI);
  await waitUntil(() => ui.editors.length === 7);
  ok(true, `Seven style sheets for ${PARENT_PROCESS_URI}`);

  info("Navigate to a page that runs in the child process");
  await navigateToAndWaitForStyleSheets(CONTENT_PROCESS_URI, ui, 2);
  // We also have to wait for the toolbox to complete the target switching
  // in order to avoid pending requests during test teardown.
  ok(
    ui.editors.every(
      editor => editor._resource.nodeHref == CONTENT_PROCESS_URI
    ),
    `Two sheets present for ${CONTENT_PROCESS_URI}`
  );

  info("Wait until the editor is ready");
  await waitFor(() => ui.selectedEditor?.sourceEditor);
});
