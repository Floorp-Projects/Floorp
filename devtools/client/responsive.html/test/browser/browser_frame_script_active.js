/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify frame script is active when expected.

const e10s = require("devtools/client/responsive.html/utils/e10s");

const TEST_URL = "http://example.com/";
add_task(async function() {
  const tab = await addTab(TEST_URL);

  let { ui } = await openRDM(tab);

  let mm = ui.getViewportBrowser().messageManager;
  let { active } = await e10s.request(mm, "IsActive");
  is(active, true, "Frame script is active");

  await closeRDM(tab);

  // Must re-get the messageManager on each run since it changes when RDM opens
  // or closes due to the design of swapFrameLoaders.  Also, we only have access
  // to a valid `ui` instance while RDM is open.
  mm = tab.linkedBrowser.messageManager;
  ({ active } = await e10s.request(mm, "IsActive"));
  is(active, false, "Frame script is active");

  // Try another round as well to be sure there is no state saved anywhere
  ({ ui } = await openRDM(tab));

  mm = ui.getViewportBrowser().messageManager;
  ({ active } = await e10s.request(mm, "IsActive"));
  is(active, true, "Frame script is active");

  await closeRDM(tab);

  // Must re-get the messageManager on each run since it changes when RDM opens
  // or closes due to the design of swapFrameLoaders.  Also, we only have access
  // to a valid `ui` instance while RDM is open.
  mm = tab.linkedBrowser.messageManager;
  ({ active } = await e10s.request(mm, "IsActive"));
  is(active, false, "Frame script is active");

  await removeTab(tab);
});
