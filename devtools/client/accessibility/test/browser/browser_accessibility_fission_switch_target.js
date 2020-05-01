/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test switching for the top-level target.

const MAIN_PROCESS_URL = "about:robots";
const MAIN_PROCESS_EXPECTED = [
  {
    expected: {
      sidebar: {
        name: "Gort! Klaatu barada nikto!",
        role: "document",
      },
    },
  },
];

const CONTENT_PROCESS_URL = buildURL(`<title>Test page</title>`);
const CONTENT_PROCESS_EXPECTED = [
  {
    expected: {
      sidebar: {
        name: "Test page",
        role: "document",
        relations: {
          "containing document": {
            role: "document",
            name: "Test page",
          },
          embeds: {
            role: "document",
            name: "Test page",
          },
        },
      },
    },
  },
];

add_task(async () => {
  await pushPref("devtools.target-switching.enabled", true);

  info(
    "Open a test page running on the content process and accessibility panel"
  );
  const env = await addTestTab(CONTENT_PROCESS_URL);
  await runA11yPanelTests(CONTENT_PROCESS_EXPECTED, env);

  info("Navigate to a page running on the main process");
  await navigateTo(MAIN_PROCESS_URL);
  await runA11yPanelTests(MAIN_PROCESS_EXPECTED, env);

  info("Back to a page running on the content process");
  await navigateTo(CONTENT_PROCESS_URL);
  await runA11yPanelTests(CONTENT_PROCESS_EXPECTED, env);

  await disableAccessibilityInspector(env);
});
