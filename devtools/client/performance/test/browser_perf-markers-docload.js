/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the sidebar is updated with "DOMContentLoaded" and "load" markers.
 */

function* spawnTest() {
  let { panel } = yield initPerformance(SIMPLE_URL);
  let { PerformanceController } = panel.panelWin;

  loadFrameScripts();

  yield startRecording(panel);
  ok(true, "Recording has started.");

  evalInDebuggee("document.location.reload()");

  yield waitUntil(() => {
    // Wait until we get the necessary markers.
    let markers = PerformanceController.getCurrentRecording().getMarkers();
    if (!markers.some(m => m.name == "document::DOMContentLoaded") ||
        !markers.some(m => m.name == "document::Load")) {
      return false;
    }

    ok(markers.filter(m => m.name == "document::DOMContentLoaded").length == 1,
      "There should only be one `DOMContentLoaded` marker.");
    ok(markers.filter(m => m.name == "document::Load").length == 1,
      "There should only be one `load` marker.");

    return true;
  });

  yield stopRecording(panel);
  ok(true, "Recording has ended.");

  yield teardown(panel);
  finish();
}

/**
 * Takes a string `script` and evaluates it directly in the content
 * in potentially a different process.
 */
function evalInDebuggee (script) {
  let { generateUUID } = Cc['@mozilla.org/uuid-generator;1'].getService(Ci.nsIUUIDGenerator);
  let deferred = Promise.defer();

  if (!mm) {
    throw new Error("`loadFrameScripts()` must be called when using MessageManager.");
  }

  let id = generateUUID().toString();
  mm.sendAsyncMessage("devtools:test:eval", { script: script, id: id });
  mm.addMessageListener("devtools:test:eval:response", handler);

  function handler ({ data }) {
    if (id !== data.id) {
      return;
    }

    mm.removeMessageListener("devtools:test:eval:response", handler);
    deferred.resolve(data.value);
  }

  return deferred.promise;
}
