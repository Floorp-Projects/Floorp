/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Load a third-party page that sets a cookie and make sure a warning about
// partitioned storage access appears in the console. Also test that multiple
// such warnings are grouped together.

"use strict";
requestLongerTimeout(2);

const TEST_PATH = "browser/devtools/client/webconsole/test/browser/";
const TEST_FILE = TEST_PATH + "test-warning-groups.html";
const TEST_URI = "http://example.com/" + TEST_FILE;

const PARTITIONED_URL =
  "https://example.org/" + TEST_PATH + "cookieSetter.html";

const STORAGE_ISOLATION_GROUP_LABEL =
  `Partitioned cookie or storage access was provided to “<URL>” because it is ` +
  `loaded in the third-party context and dynamic state partitioning is enabled.`;

const COOKIE_BEHAVIOR_PREF = "network.cookie.cookieBehavior";
const COOKIE_BEHAVIOR_PARTITION_FOREIGN = 5;

pushPref("devtools.webconsole.groupWarningMessages", true);

async function cleanUp() {
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
}

add_task(async function testStorageIsolationMessage() {
  await cleanUp();

  await pushPref(COOKIE_BEHAVIOR_PREF, COOKIE_BEHAVIOR_PARTITION_FOREIGN);
  const { hud, tab, win } = await openNewWindowAndConsole(TEST_URI);
  const now = Date.now();

  const getWarningMessage = url =>
    STORAGE_ISOLATION_GROUP_LABEL.replace("<URL>", url);

  info("Test storage isolation message");
  const url1 = `${PARTITIONED_URL}?1&${now}`;
  const message = getWarningMessage(url1);
  const onStorageIsolationWarningMessage = waitForMessageByType(
    hud,
    message,
    ".warn"
  );
  emitStorageIsolationMessage(tab, url1);
  await onStorageIsolationWarningMessage;

  ok(true, "The storage isolation message was displayed");

  info(
    "Emit a new storage isolation message to check that it causes a grouping"
  );
  const onStorageIsolationWarningGroupMessage = waitForMessageByType(
    hud,
    STORAGE_ISOLATION_GROUP_LABEL,
    ".warn"
  );
  const url2 = `${PARTITIONED_URL}?2&${now}`;
  emitStorageIsolationMessage(tab, url2);
  const { node } = await onStorageIsolationWarningGroupMessage;
  is(
    node.querySelector(".warning-group-badge").textContent,
    "2",
    "The badge has the expected text"
  );

  await checkConsoleOutputForWarningGroup(hud, [
    `▶︎⚠ ${STORAGE_ISOLATION_GROUP_LABEL} 2`,
  ]);

  info("Open the group");
  node.querySelector(".arrow").click();
  await waitFor(() => findWarningMessage(hud, url1));

  await checkConsoleOutputForWarningGroup(hud, [
    `▼︎⚠ ${STORAGE_ISOLATION_GROUP_LABEL} 2`,
    `| ${getWarningMessage(url1)}`,
    `| ${getWarningMessage(url2)}`,
  ]);
  await win.close();

  await cleanUp();
});

/**
 * Emit a Storage Isolation message. This is done by loading an iframe with a
 * third-party resource. The iframe is loaded with a incremented counter query
 * parameter each time so we can get the warning message.
 */
function emitStorageIsolationMessage(tab, url) {
  SpecialPowers.spawn(tab.linkedBrowser, [url], function (innerURL) {
    content.wrappedJSObject.loadIframe(innerURL);
  });
}
