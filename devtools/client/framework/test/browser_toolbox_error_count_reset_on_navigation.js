/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
// Test for error count in toolbar when navigating and webconsole isn't enabled
const TEST_URI = `http://example.org/document-builder.sjs?html=<meta charset=utf8></meta>
<script>
  console.error("Cache Error1");
  console.exception(false, "Cache Exception");
  console.warn("Cache warning");
  console.assert(false, "Cache assert");
  cache.unknown.access
</script>`;

const { Toolbox } = require("resource://devtools/client/framework/toolbox.js");

add_task(async function () {
  // Disable bfcache for Fission for now.
  // If Fission is disabled, the pref is no-op.
  await SpecialPowers.pushPrefEnv({
    set: [["fission.bfcacheInParent", false]],
  });

  // Make sure we start the test with the split console disabled.
  // ⚠️ In this test it's important to _not_ enable the console.
  await pushPref("devtools.toolbox.splitconsoleEnabled", false);
  const tab = await addTab(TEST_URI);

  const toolbox = await openToolboxForTab(
    tab,
    "inspector",
    Toolbox.HostType.BOTTOM
  );

  info("Check for cached errors");
  // (console.error + console.exception + console.assert + error)
  const expectedErrorCount = 4;

  await waitFor(() => getErrorIcon(toolbox));
  is(
    getErrorIcon(toolbox).getAttribute("title"),
    "Show Split Console",
    "Icon has expected title"
  );
  is(
    getErrorIconCount(toolbox),
    expectedErrorCount,
    "Correct count is displayed"
  );

  info("Add another error so we have a different count");
  ContentTask.spawn(tab.linkedBrowser, null, function () {
    content.console.error("Live Error1");
  });

  const newExpectedErrorCount = expectedErrorCount + 1;
  await waitFor(() => getErrorIconCount(toolbox) === newExpectedErrorCount);

  info(
    "Reload the page and check that the error icon has the expected content"
  );
  await reloadBrowser();

  await waitFor(
    () => getErrorIconCount(toolbox) === expectedErrorCount,
    "Error count is cleared on navigation and then populated with the expected number of errors"
  );
  ok(true, "Correct count is displayed");

  info(
    "Navigate to an error-less page and check that the error icon is hidden"
  );
  await navigateTo(`data:text/html;charset=utf8,No errors`);
  await waitFor(
    () => !getErrorIcon(toolbox),
    "Error count is cleared on navigation"
  );
  ok(
    true,
    "The error icon was hidden when navigating to a new page without errors"
  );

  toolbox.destroy();
});
