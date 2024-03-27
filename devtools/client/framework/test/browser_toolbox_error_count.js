/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
// Test for error icon and the error count displayed at right of the
// toolbox toolbar

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/webconsole/test/browser/shared-head.js",
  this
);

const TEST_URI = `https://example.com/document-builder.sjs?html=<meta charset=utf8></meta>
<script>
  console.error("Cache Error1");
  console.exception(false, "Cache Exception");
  console.warn("Cache warning");
  console.assert(false, "Cache assert");
  cache.unknown.access
</script><body>`;

const { Toolbox } = require("resource://devtools/client/framework/toolbox.js");

add_task(async function () {
  // Make sure we start the test with the split console closed, and the split console setting enabled
  await pushPref("devtools.toolbox.splitconsole.open", false);
  await pushPref("devtools.toolbox.splitconsole.enabled", true);

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("devtools.toolbox.splitconsole.enabled");
  });

  const tab = await addTab(TEST_URI);

  const toolbox = await openToolboxForTab(
    tab,
    "inspector",
    Toolbox.HostType.BOTTOM
  );

  info("Check for cached errors");
  // (console.error + console.exception + console.assert + error)
  let expectedErrorCount = 4;

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

  info("Check that calling console.clear clears the error count");
  ContentTask.spawn(tab.linkedBrowser, null, function () {
    content.console.clear();
  });
  await waitFor(
    () => !getErrorIcon(toolbox),
    "Wait until the error button hides"
  );
  ok(true, "The button was hidden after calling console.clear()");

  info("Check that realtime errors increase the counter");
  ContentTask.spawn(tab.linkedBrowser, null, function () {
    content.console.error("Live Error1");
    content.console.error("Live Error2");
    content.console.exception("Live Exception");
    content.console.warn("Live warning");
    content.console.assert(false, "Live assert");
    content.fetch("unknown-url-that-will-404");
    const script = content.document.createElement("script");
    script.textContent = `a.b.c.d`;
    content.document.body.append(script);
  });

  expectedErrorCount = 6;
  await waitFor(() => getErrorIconCount(toolbox) === expectedErrorCount);

  info("Check if split console opens on clicking the error icon");
  const onSplitConsoleOpen = toolbox.once("split-console");
  getErrorIcon(toolbox).click();
  await onSplitConsoleOpen;
  ok(
    toolbox.splitConsole,
    "The split console was opened after clicking on the icon."
  );

  // Select the console and check that the icon title is updated
  await toolbox.selectTool("webconsole");
  is(
    getErrorIcon(toolbox).getAttribute("title"),
    null,
    "When the console is selected, the icon does not have a title"
  );

  const hud = toolbox.getCurrentPanel().hud;
  const webconsoleDoc = hud.ui.window.document;
  // wait until all error messages are displayed in the console
  await waitFor(
    async () => (await findAllErrors(hud)).length === expectedErrorCount
  );

  info("Clear the console output and check that the error icon is hidden");
  webconsoleDoc.querySelector(".devtools-clear-icon").click();
  await waitFor(() => !getErrorIcon(toolbox));
  ok(true, "Clearing the console does hide the icon");
  await waitFor(async () => (await findAllErrors(hud)).length === 0);

  info("Check that the error count is capped at 99");
  expectedErrorCount = 100;
  ContentTask.spawn(tab.linkedBrowser, expectedErrorCount, function (count) {
    for (let i = 0; i < count; i++) {
      content.console.error(i);
    }
  });

  // Wait until all the messages are displayed in the console
  await waitFor(
    async () => (await findAllErrors(hud)).length === expectedErrorCount
  );

  await waitFor(() => getErrorIconCount(toolbox) === "99+");
  ok(true, "The message count doesn't go higher than 99");

  info(
    "Reload the page and check that the error icon has the expected content"
  );
  await reloadBrowser();

  // (console.error, console.exception, console.assert and exception)
  expectedErrorCount = 4;
  await waitFor(() => getErrorIconCount(toolbox) === expectedErrorCount);
  ok(true, "Correct count is displayed");

  // wait until all error messages are displayed in the console
  await waitFor(
    async () => (await findAllErrors(hud)).length === expectedErrorCount
  );

  info("Disable the error icon from the options panel");
  const onOptionsSelected = toolbox.once("options-selected");
  toolbox.selectTool("options");
  const optionsPanel = await onOptionsSelected;
  const errorCountButtonToggleEl = optionsPanel.panelWin.document.querySelector(
    "input#command-button-errorcount"
  );
  errorCountButtonToggleEl.click();

  await waitFor(() => !getErrorIcon(toolbox));
  ok(true, "The error icon hides when disabling it from the settings panel");

  info("Check that emitting new errors don't show the icon");
  ContentTask.spawn(tab.linkedBrowser, null, function () {
    content.console.error("Live Error1 while disabled");
    content.console.error("Live Error2 while disabled");
  });

  expectedErrorCount = expectedErrorCount + 2;
  // Wait until messages are displayed in the console, so the toolbar would have the time
  // to render the error icon again.
  await toolbox.selectTool("webconsole");
  await waitFor(
    async () => (await findAllErrors(hud)).length === expectedErrorCount
  );
  is(
    getErrorIcon(toolbox),
    null,
    "The icon is still hidden even after generating new errors"
  );

  info("Re-enable the error icon");
  await toolbox.selectTool("options");
  errorCountButtonToggleEl.click();
  await waitFor(() => getErrorIconCount(toolbox) === expectedErrorCount);
  ok(
    true,
    "The error is displayed again, with the correct error count, after enabling it from the settings panel"
  );

  info("Disable the split console from the options panel");
  const splitConsoleButtonToggleEl =
    optionsPanel.panelWin.document.querySelector(
      "input#devtools-enable-split-console"
    );
  splitConsoleButtonToggleEl.click();
  await waitFor(
    () => getErrorIcon(toolbox).getAttribute("title") === "Show Console"
  );
  ok(
    true,
    "The error count icon title changed to reflect split console being disabled"
  );

  info(
    "Check if with split console being disabled click leads to the console tab"
  );
  const onWebConsole = toolbox.once("webconsole-selected");
  getErrorIcon(toolbox).click();
  await onWebConsole;
  ok(!toolbox.splitConsole, "Web Console opened instead of split console");

  toolbox.destroy();
});

function findAllErrors(hud) {
  return findMessagesVirtualizedByType({ hud, typeSelector: ".error" });
}
