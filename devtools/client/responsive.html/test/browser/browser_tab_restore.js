/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify RDM tab reopens to content (with RDM closed) when restoring the tab.

const TEST_URL = "http://example.com/";

const ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);
const { TabStateFlusher } = Cu.import("resource:///modules/sessionstore/TabStateFlusher.jsm", {});

add_task(function* () {
  // Open tab, start RDM, close tab
  let tab = yield addTab(TEST_URL);
  // Ensure tab state is flushed to session store before closing (so it can be restored)
  yield TabStateFlusher.flush(tab.linkedBrowser);
  let { ui } = yield openRDM(tab);
  yield removeTab(tab);
  is(ui.destroyed, true, "RDM closed");

  // Restore tab
  tab = ss.undoCloseTab(window, 0);
  yield once(tab, "SSTabRestored");

  // Check location
  is(tab.linkedBrowser.documentURI.spec, TEST_URL, "Restored tab location to test page");

  yield removeTab(tab);
});
