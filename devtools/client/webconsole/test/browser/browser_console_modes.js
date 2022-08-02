/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that messages from different contexts appears in the Browser Console and that their
// visibility can be controlled through the UI (either with the ChromeDebugToolbar mode whe
// Fission is enabled, or through the "Show Content Messages" setting when it's not).

"use strict";

const FILTER_PREFIX = "BC_TEST|";

const contentArgs = {
  log: FILTER_PREFIX + "MyLog",
  warn: FILTER_PREFIX + "MyWarn",
  error: FILTER_PREFIX + "MyError",
  exception: FILTER_PREFIX + "MyException",
  info: FILTER_PREFIX + "MyInfo",
  debug: FILTER_PREFIX + "MyDebug",
  counterName: FILTER_PREFIX + "MyCounter",
  timerName: FILTER_PREFIX + "MyTimer",
};

const TEST_URI = `data:text/html,<!DOCTYPE html><meta charset=utf8>console API calls<script>
  console.log("${contentArgs.log}", {hello: "world"});
  console.warn("${contentArgs.warn}", {hello: "world"});
  console.error("${contentArgs.error}", {hello: "world"});
  console.exception("${contentArgs.exception}", {hello: "world"});
  console.info("${contentArgs.info}", {hello: "world"});
  console.debug("${contentArgs.debug}", {hello: "world"});
  console.count("${contentArgs.counterName}");
  console.time("${contentArgs.timerName}");
  console.timeLog("${contentArgs.timerName}", "MyTimeLog", {hello: "world"});
  console.timeEnd("${contentArgs.timerName}");
  console.trace("${FILTER_PREFIX}", {hello: "world"});
  console.assert(false, "${FILTER_PREFIX}", {hello: "world"});
  console.table(["${FILTER_PREFIX}", {hello: "world"}]);
</script>`;

// Test can be a bit long
requestLongerTimeout(2);

add_task(async function() {
  // Show the content messages
  await pushPref("devtools.browserconsole.contentMessages", true);
  await pushPref("devtools.browsertoolbox.scope", "everything");
  // Show context selector
  await pushPref("devtools.chrome.enabled", true);
  await pushPref("devtools.webconsole.input.context", true);

  info("Run once with Fission enabled");
  await checkMessages(true);

  info("Run once with Fission disabled");
  await checkMessages(false);
});

async function checkMessages(isMultiprocessBrowserConsole) {
  await pushPref(
    "devtools.browsertoolbox.fission",
    isMultiprocessBrowserConsole
  );

  // Open the WebConsole on the tab to check changing mode won't focus the tab
  await openNewTabAndConsole(TEST_URI);

  // Open the Browser Console
  const hud = await BrowserConsoleManager.toggleBrowserConsole();
  // Set a filter to have predictable results, otherwise we may get messages from Firefox
  // polluting the test.
  await setFilterState(hud, { text: FILTER_PREFIX });

  // In non fission world, we don't retrieve cached messages, so we need to reload the
  // tab to see them.
  if (!isMultiprocessBrowserConsole) {
    await reloadBrowser();
  }

  const chromeDebugToolbar = hud.ui.document.querySelector(
    ".chrome-debug-toolbar"
  );
  if (isMultiprocessBrowserConsole) {
    ok(
      !!chromeDebugToolbar,
      "ChromeDebugToolbar is displayed when the Browser Console has fission support"
    );
    ok(
      !hud.chromeWindow.document.querySelector(
        ".webconsole-console-settings-menu-item-contentMessages"
      ),
      "Show content messages menu item isn't displayed when Browser Console has Fission support"
    );
    is(
      hud.chromeWindow.document.title,
      "Multiprocess Browser Console",
      "Browser Console window has expected title"
    );
  } else {
    ok(
      !chromeDebugToolbar,
      "ChromeDebugToolbar is not displayed when the Browser Console does not have fission support"
    );
    ok(
      hud.chromeWindow.document.querySelector(
        ".webconsole-console-settings-menu-item-contentMessages"
      ),
      "Show content messages menu item is displayed when Browser Console does not have Fission support"
    );
    is(
      hud.chromeWindow.document.title,
      "Browser Console",
      "Browser Console window has expected title"
    );
  }

  let evaluationContextSelectorButton;
  if (isMultiprocessBrowserConsole) {
    evaluationContextSelectorButton = await waitFor(() =>
      hud.ui.outputNode.querySelector(".webconsole-evaluation-selector-button")
    );

    info("Select the content process target");
    const pid =
      gBrowser.selectedTab.linkedBrowser.browsingContext.currentWindowGlobal
        .osPid;
    getContextSelectorItems(hud)
      .find(item =>
        item.querySelector(".label")?.innerText?.startsWith(`(pid ${pid})`)
      )
      .click();

    await waitFor(() =>
      evaluationContextSelectorButton.classList.contains("checked")
    );

    await executeAndWaitForResultMessage(
      hud,
      `Cu.reportError("${FILTER_PREFIX}Content Cu.reportError");21+21`,
      42
    );
  } else {
    await SpecialPowers.spawn(
      gBrowser.selectedTab.linkedBrowser,
      [FILTER_PREFIX],
      prefix => Cu.reportError(prefix + "Content Cu.reportError")
    );
  }

  // Since we run the non-fission test in second, and given we don't retrieve cached messages
  // in such case, it's okay to just cause the message to appear in this function.
  Cu.reportError(FILTER_PREFIX + "Parent Cu.reportError");

  await waitFor(() => findErrorMessage(hud, "Parent Cu.reportError"));
  ok(true, "Parent process message is displayed");

  await waitFor(() => findErrorMessage(hud, "Content Cu.reportError"));
  ok(true, "reportError message from content process is displayed");

  const suffix = isMultiprocessBrowserConsole
    ? ` Object { hello: "world" }`
    : "";
  const expectedMessages = [
    contentArgs.log + suffix,
    contentArgs.warn + suffix,
    contentArgs.error + suffix,
    contentArgs.exception + suffix,
    contentArgs.info + suffix,
    contentArgs.debug + suffix,
    `${contentArgs.counterName}: 1`,
    `MyTimeLog${suffix}`,
    `timer ended`,
    `console.trace() ${FILTER_PREFIX}${suffix}`,
    `Assertion failed: ${FILTER_PREFIX}${suffix}`,
  ];

  // console.table is rendered as <unavailable> in non-fission browser console.
  if (isMultiprocessBrowserConsole) {
    expectedMessages.push("console.table()");
  }

  info("wait for all the messages to be displayed");
  await waitFor(
    () =>
      expectedMessages.every(
        expectedMessage =>
          findMessagePartsByType(hud, {
            text: expectedMessage,
            typeSelector: ".console-api",
            partSelector: ".message-body",
          }).length == 1
      ),
    "wait for all the messages to be displayed",
    100
  );
  ok(true, "Expected messages are displayed in the browser console");

  if (isMultiprocessBrowserConsole) {
    const tableMessage = findConsoleAPIMessage(
      hud,
      "console.table()",
      ".table"
    );

    const table = await waitFor(() =>
      tableMessage.querySelector(".consoletable")
    );
    ok(table, "There is a table element");
    const tableTextContent = table.textContent;
    ok(
      tableTextContent.includes(FILTER_PREFIX) &&
        tableTextContent.includes(`world`) &&
        tableTextContent.includes(`hello`),
      "Table has expected content"
    );
  }

  if (isMultiprocessBrowserConsole) {
    info("Set Browser Console Mode to parent process only");
    chromeDebugToolbar
      .querySelector(
        `.chrome-debug-toolbar input[name="chrome-debug-mode"][value="parent-process"]`
      )
      .click();
  } else {
    info("Uncheck the Show content messages checkbox");
    await toggleConsoleSetting(
      hud,
      ".webconsole-console-settings-menu-item-contentMessages"
    );
  }
  info("Wait for content messages to be hidden");
  await waitFor(() => !findConsoleAPIMessage(hud, contentArgs.log));

  for (const expectedMessage of expectedMessages) {
    ok(
      !findConsoleAPIMessage(hud, expectedMessage),
      `"${expectedMessage}" is hidden`
    );
  }

  if (isMultiprocessBrowserConsole) {
    is(
      hud.chromeWindow.document.title,
      "Parent process Browser Console",
      "Browser Console window title was updated"
    );

    ok(
      hud.iframeWindow.document.hasFocus(),
      "Browser Console is still focused"
    );

    await waitFor(
      () => !evaluationContextSelectorButton.classList.contains("checked")
    );
    ok(true, "Changing mode did reset the context selector");
    ok(
      findMessageByType(hud, "21+21", ".command"),
      "The evaluation message is still displayed"
    );
    ok(
      findEvaluationResultMessage(hud, `42`),
      "The evaluation result is still displayed"
    );
  }

  info(
    "Check that message from parent process is still visible in the Browser Console"
  );
  ok(
    !!findErrorMessage(hud, "Parent Cu.reportError"),
    "Parent process message is still displayed"
  );

  if (isMultiprocessBrowserConsole) {
    info("Set Browser Console Mode to Multiprocess");
    chromeDebugToolbar
      .querySelector(
        `.chrome-debug-toolbar input[name="chrome-debug-mode"][value="everything"]`
      )
      .click();
  } else {
    info("Check the Show content messages checkbox");
    await toggleConsoleSetting(
      hud,
      ".webconsole-console-settings-menu-item-contentMessages"
    );
  }
  info("Wait for content messages to be displayed");
  await waitFor(() =>
    expectedMessages.every(expectedMessage =>
      findConsoleAPIMessage(hud, expectedMessage)
    )
  );

  for (const expectedMessage of expectedMessages) {
    // Search into the message body as the message location could have some of the
    // searched text.
    is(
      findMessagePartsByType(hud, {
        text: expectedMessage,
        typeSelector: ".console-api",
        partSelector: ".message-body",
      }).length,
      1,
      `"${expectedMessage}" is visible`
    );
  }

  is(
    findErrorMessages(hud, "Content Cu.reportError").length,
    1,
    "reportError message from content process is only displayed once"
  );

  if (isMultiprocessBrowserConsole) {
    is(
      hud.chromeWindow.document.title,
      "Multiprocess Browser Console",
      "Browser Console window title was updated again"
    );
  }

  info("Clear and close the Browser Console");
  await safeCloseBrowserConsole({ clearOutput: true });

  // Since we directly log the error from a SpecialPower task, we need to clear the cache
  // from the same process, as the Browser Console does not have a connection to it when
  // Fission is not supported.
  if (!isMultiprocessBrowserConsole) {
    await SpecialPowers.spawn(gBrowser.selectedTab.linkedBrowser, [], () =>
      Services.console.reset()
    );
  }
}
