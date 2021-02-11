/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that console API calls in the content page appear in the browser console.

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

const TEST_URI = `data:text/html,<meta charset=utf8>console API calls<script>
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

add_task(async function() {
  // Show the content messages
  await pushPref("devtools.browserconsole.contentMessages", true);

  info("Run once with Fission enabled");
  await pushPref("devtools.browsertoolbox.fission", true);
  await checkContentConsoleApiMessages(true);

  info("Run once with Fission disabled");
  await pushPref("devtools.browsertoolbox.fission", false);
  await checkContentConsoleApiMessages(false);
});

async function checkContentConsoleApiMessages(nonPrimitiveVariablesDisplayed) {
  // Add the tab first so it creates the ContentProcess
  const tab = await addTab(TEST_URI);

  // Open the Browser Console
  const hud = await BrowserConsoleManager.toggleBrowserConsole();
  await setFilterState(hud, { text: FILTER_PREFIX });

  // In non fission world, we don't retrieve cached messages, so we need to reload the
  // tab to see them.
  if (!nonPrimitiveVariablesDisplayed) {
    const loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
    tab.linkedBrowser.reload();
    await loaded;
  }

  const suffix = nonPrimitiveVariablesDisplayed
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
  if (nonPrimitiveVariablesDisplayed) {
    expectedMessages.push("console.table()");
  }

  info("wait for all the messages to be displayed");
  await waitFor(
    () =>
      expectedMessages.every(expectedMessage =>
        findMessage(hud, expectedMessage)
      ),
    "wait for all the messages to be displayed",
    100
  );
  ok(true, "Expected messages are displayed in the browser console");

  if (nonPrimitiveVariablesDisplayed) {
    const tableMessage = findMessage(hud, "console.table()", ".message.table");

    const table = await waitFor(() =>
      tableMessage.querySelector(".new-consoletable")
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

  info("Uncheck the Show content messages checkbox");
  const onContentMessagesHidden = waitFor(
    () => !findMessage(hud, contentArgs.log)
  );
  await toggleConsoleSetting(
    hud,
    ".webconsole-console-settings-menu-item-contentMessages"
  );
  await onContentMessagesHidden;

  for (const expectedMessage of expectedMessages) {
    ok(!findMessage(hud, expectedMessage), `"${expectedMessage}" is hidden`);
  }

  info("Check the Show content messages checkbox");
  const onContentMessagesDisplayed = waitFor(() =>
    expectedMessages.every(expectedMessage => findMessage(hud, expectedMessage))
  );
  await toggleConsoleSetting(
    hud,
    ".webconsole-console-settings-menu-item-contentMessages"
  );
  await onContentMessagesDisplayed;

  for (const expectedMessage of expectedMessages) {
    ok(findMessage(hud, expectedMessage), `"${expectedMessage}" is visible`);
  }

  info("Clear and close the Browser Console");
  await safeCloseBrowserConsole({ clearOutput: true });
}
