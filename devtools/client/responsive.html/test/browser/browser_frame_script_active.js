/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify frame script is active when expected.

const e10s = require("devtools/client/responsive.html/utils/e10s");

const TEST_URL = "http://example.com/";
add_task(function* () {
  let tab = yield addTab(TEST_URL);

  let { ui } = yield openRDM(tab);

  let mm = ui.getViewportBrowser().messageManager;
  let { active } = yield e10s.request(mm, "IsActive");
  is(active, true, "Frame script is active");

  yield closeRDM(tab);

  // Must re-get the messageManager on each run since it changes when RDM opens
  // or closes due to the design of swapFrameLoaders.  Also, we only have access
  // to a valid `ui` instance while RDM is open.
  mm = tab.linkedBrowser.messageManager;
  ({ active } = yield e10s.request(mm, "IsActive"));
  is(active, false, "Frame script is active");

  // Try another round as well to be sure there is no state saved anywhere
  ({ ui } = yield openRDM(tab));

  mm = ui.getViewportBrowser().messageManager;
  ({ active } = yield e10s.request(mm, "IsActive"));
  is(active, true, "Frame script is active");

  yield closeRDM(tab);

  // Must re-get the messageManager on each run since it changes when RDM opens
  // or closes due to the design of swapFrameLoaders.  Also, we only have access
  // to a valid `ui` instance while RDM is open.
  mm = tab.linkedBrowser.messageManager;
  ({ active } = yield e10s.request(mm, "IsActive"));
  is(active, false, "Frame script is active");

  yield removeTab(tab);
});
