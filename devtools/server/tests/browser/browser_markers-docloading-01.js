/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get DOMContentLoaded and Load markers
 */

const { TimelineFront } = require("devtools/server/actors/timeline");
const MARKER_NAMES = ["document::DOMContentLoaded", "document::Load"];

add_task(function* () {
  let browser = yield addTab(MAIN_DOMAIN + "doc_innerHTML.html");
  let doc = browser.contentDocument;

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let front = TimelineFront(client, form);
  let rec = yield front.start({ withMarkers: true });

  front.once("doc-loading", e => {
    ok(false, "Should not be emitting doc-loading events.");
  });

  executeSoon(() => doc.location.reload());

  yield waitForMarkerType(front, MARKER_NAMES, () => true, e => e, "markers");
  yield front.stop(rec);

  ok(true, "Found the required marker names.");

  // Wait some more time to make sure the 'doc-loading' events never get fired.
  yield DevToolsUtils.waitForTime(1000);

  yield closeDebuggerClient(client);
  gBrowser.removeCurrentTab();
});
