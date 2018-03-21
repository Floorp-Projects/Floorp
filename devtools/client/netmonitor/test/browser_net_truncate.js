/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Verifies that truncated response bodies still have the correct reported size.
 */
add_task(async function() {
  let limit = Services.prefs.getIntPref("devtools.netmonitor.responseBodyLimit");
  let URL = EXAMPLE_URL + "sjs_truncate-test-server.sjs?limit=" + limit;
  let { monitor, tab } = await initNetMonitor(URL);

  info("Starting test... ");

  let { L10N } = require("devtools/client/netmonitor/src/utils/l10n");

  let { document } = monitor.panelWin;

  let wait = waitForNetworkEvents(monitor, 1);
  tab.linkedBrowser.reload();
  await wait;

  // Response content will be updated asynchronously, we should make sure data is updated
  // on DOM before asserting.
  await waitUntil(() => document.querySelector(".request-list-item"));
  let item = document.querySelectorAll(".request-list-item")[0];
  await waitUntil(() => item.querySelector(".requests-list-type").title);

  let type = item.querySelector(".requests-list-type").textContent;
  let fullMimeType = item.querySelector(".requests-list-type").title;
  let transferred = item.querySelector(".requests-list-transferred").textContent;
  let size = item.querySelector(".requests-list-size").textContent;

  is(type, "plain", "Type should be rendered correctly.");
  is(fullMimeType, "text/plain; charset=utf-8",
    "Mimetype should be rendered correctly.");
  is(transferred, L10N.getFormatStrWithNumbers("networkMenu.sizeMB", 2),
    "Transferred size should be rendered correctly.");
  is(size, L10N.getFormatStrWithNumbers("networkMenu.sizeMB", 2),
    "Size should be rendered correctly.");

  return teardown(monitor);
});
