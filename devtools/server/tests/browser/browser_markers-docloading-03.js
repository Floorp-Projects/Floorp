/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get DOMContentLoaded and Load markers
 */
"use strict";

const { TimelineFront } = require("devtools/shared/fronts/timeline");
const MARKER_NAMES = ["document::DOMContentLoaded", "document::Load"];

add_task(function* () {
  let browser = yield addTab(MAIN_DOMAIN + "doc_innerHTML.html");
  // eslint-disable-next-line mozilla/no-cpows-in-tests
  let doc = browser.contentDocument;

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let front = TimelineFront(client, form);
  let rec = yield front.start({ withDocLoadingEvents: true });

  waitForMarkerType(front, MARKER_NAMES, () => true, e => e, "markers").then(e => {
    ok(false, "Should not be emitting doc-loading markers.");
  });

  yield new Promise(resolve => {
    front.once("doc-loading", resolve);
    doc.location.reload();
  });

  ok(true, "At least one doc-loading event got fired.");

  yield front.stop(rec);

  // Wait some more time to make sure the 'doc-loading' markers never get fired.
  yield DevToolsUtils.waitForTime(1000);

  yield client.close();
  gBrowser.removeCurrentTab();
});
