/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 1201597 - Sanity test that we can take a heap snapshot in an e10s child process.
 */

"use strict";

add_task(async function() {
  // Create a minimal browser with a message manager
  const browser = document.createXULElement("browser");
  browser.setAttribute("type", "content");
  document.body.appendChild(browser);

  const mm = browser.messageManager;

  function onError(e) {
    ok(false, e.data.error);
  }
  mm.addMessageListener("testSaveHeapSnapshot:error", onError);

  const onDone = new Promise(resolve => {
    mm.addMessageListener("testSaveHeapSnapshot:done", function onMsg() {
      mm.removeMessageListener("testSaveHeapSnapshot:done", onMsg);
      mm.removeMessageListener("testSaveHeapSnapshot:error", onError);
      ok(true, "Saved heap snapshot in child process");
      resolve();
    });
  });

  info("Loading frame script to save heap snapshot");
  // This function is stringified and loaded in the child process as a frame
  // script.
  function childFrameScript() {
    /* global sendAsyncMessage */
    try {
      ChromeUtils.saveHeapSnapshot({ runtime: true });
    } catch (err) {
      sendAsyncMessage("testSaveHeapSnapshot:error", { error: err.toString() });
      return;
    }

    sendAsyncMessage("testSaveHeapSnapshot:done", {});
  }
  mm.loadFrameScript(
    "data:,(" + encodeURI(childFrameScript.toString()) + ")();",
    false
  );

  await onDone;

  browser.remove();
});
