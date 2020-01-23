/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test switching for the top-level target.

const PARENT_PROCESS_URI = "about:robots";
const CONTENT_PROCESS_URI = TEST_BASE_HTTPS + "simple.html";

add_task(async function() {
  await pushPref("devtools.target-switching.enabled", true);

  // We use about:robots, because this page will run in the parent process.
  // Navigating from about:robots to a regular content page will always trigger a target
  // switch, with or without fission.

  info("Open a page that runs in the parent process");
  const { toolbox, ui } = await openStyleEditorForURL(PARENT_PROCESS_URI);
  is(ui.editors.length, 3, `Three style sheets for ${PARENT_PROCESS_URI}`);

  info("Navigate to a page that runs in the child process");
  await navigateToAndWaitForStyleSheets(CONTENT_PROCESS_URI, ui);
  is(ui.editors.length, 2, `Two sheets present for ${CONTENT_PROCESS_URI}`);

  await toolbox.closeToolbox();
});
