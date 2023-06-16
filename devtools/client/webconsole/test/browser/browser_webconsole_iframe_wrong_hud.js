/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Ensure that iframes are not associated with the wrong hud. See Bug 593003.

const TEST_URI =
  "https://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-iframe-wrong-hud.html";

const TEST_IFRAME_URI =
  "https://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-iframe-wrong-hud-iframe.html";

const TEST_DUMMY_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-console.html";

add_task(async function () {
  await pushPref("devtools.webconsole.filter.net", true);
  const tab1 = await addTab(TEST_URI);
  const hud1 = await openConsole(tab1);

  const tab2 = await addTab(TEST_DUMMY_URI);
  await openConsole(gBrowser.selectedTab);

  info("Reloading tab 1");
  await reloadBrowser({ browser: tab1.linkedBrowser });

  info("Waiting for messages");
  await waitFor(() => findMessageByType(hud1, TEST_IFRAME_URI, ".network"));

  const hud2 = await openConsole(tab2);
  is(
    findMessageByType(hud2, TEST_IFRAME_URI, ".network"),
    undefined,
    "iframe network request is not displayed in tab2"
  );
});
