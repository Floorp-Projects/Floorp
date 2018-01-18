/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Ensure that iframes are not associated with the wrong hud. See Bug 593003.

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
  "new-console-output/test/mochitest/test-iframe-wrong-hud.html";

const TEST_IFRAME_URI = "http://example.com/browser/devtools/client/webconsole/" +
  "new-console-output/test/mochitest/test-iframe-wrong-hud-iframe.html";

const TEST_DUMMY_URI = "http://example.com/browser/devtools/client/webconsole/" +
  "new-console-output/test/mochitest/test-console.html";

add_task(async function () {
  await pushPref("devtools.webconsole.filter.net", true);
  const tab1 = await addTab(TEST_URI);
  const hud1 = await openConsole(tab1);

  const tab2 = await addTab(TEST_DUMMY_URI);
  await openConsole(gBrowser.selectedTab);

  info("Reloading tab 1");
  await reloadTab(tab1);

  info("Waiting for messages");
  await waitFor(() => findMessage(hud1, TEST_IFRAME_URI, ".message.network"));

  const hud2 = await openConsole(tab2);
  is(findMessage(hud2, TEST_IFRAME_URI), null,
    "iframe network request is not displayed in tab2");
});

function reloadTab(tab) {
  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  tab.linkedBrowser.reload();
  return loaded;
}
