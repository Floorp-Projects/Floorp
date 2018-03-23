/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get DOMContentLoaded and Load markers
 */
"use strict";

const { TimelineFront } = require("devtools/shared/fronts/timeline");
const MARKER_NAMES = ["document::DOMContentLoaded", "document::Load"];

add_task(async function() {
  let browser = await addTab(MAIN_DOMAIN + "doc_innerHTML.html");

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = await connectDebuggerClient(client);
  let front = TimelineFront(client, form);
  let rec = await front.start({ withDocLoadingEvents: true });

  waitForMarkerType(front, MARKER_NAMES, () => true, e => e, "markers").then(e => {
    ok(false, "Should not be emitting doc-loading markers.");
  });

  await new Promise(resolve => {
    front.once("doc-loading", resolve);
    ContentTask.spawn(browser, null, function() {
      content.location.reload();
    });
  });

  ok(true, "At least one doc-loading event got fired.");

  await front.stop(rec);

  // Wait some more time to make sure the 'doc-loading' markers never get fired.
  await DevToolsUtils.waitForTime(1000);

  await client.close();
  gBrowser.removeCurrentTab();
});
