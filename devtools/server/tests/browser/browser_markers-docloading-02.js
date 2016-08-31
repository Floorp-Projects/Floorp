/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we get DOMContentLoaded and Load markers
 */

const { TimelineFront } = require("devtools/shared/fronts/timeline");
const MARKER_NAMES = ["document::DOMContentLoaded", "document::Load"];

add_task(function* () {
  let browser = yield addTab(MAIN_DOMAIN + "doc_innerHTML.html");
  let doc = browser.contentDocument;

  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = yield connectDebuggerClient(client);
  let front = TimelineFront(client, form);
  let rec = yield front.start({ withMarkers: true, withDocLoadingEvents: true });

  yield new Promise(resolve => {
    front.once("doc-loading", resolve);
    doc.location.reload();
  });

  ok(true, "At least one doc-loading event got fired.");

  yield waitForMarkerType(front, MARKER_NAMES, () => true, e => e, "markers");
  yield front.stop(rec);

  ok(true, "Found the required marker names.");

  yield client.close();
  gBrowser.removeCurrentTab();
});
