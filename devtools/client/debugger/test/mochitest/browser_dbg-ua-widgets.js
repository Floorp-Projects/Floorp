/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Test that you can debug User Agent widgets (like video controls)
// only when enabling "devtools.inspector.showAllAnonymousContent" pref.

"use strict";

const TEST_URL = "data:text/html,<video controls>";

add_task(async function () {
  await pushPref("devtools.inspector.showAllAnonymousContent", false);
  let dbg = await initDebuggerWithAbsoluteURL(TEST_URL);
  ok(
    !sourceExists(dbg, "videocontrols.js"),
    "Video controls internal file is *not* visibile in the debugger when the pref is false"
  );
  await dbg.toolbox.closeToolbox();

  info("Toggle the pref to true");
  await pushPref("devtools.inspector.showAllAnonymousContent", true);

  dbg = await initDebuggerWithAbsoluteURL(TEST_URL);
  ok(
    sourceExists(dbg, "videocontrols.js"),
    "Video controls internal file *is* visibile in the debugger when the pref is true"
  );

  clickElement(dbg, "pause");
  await waitForState(dbg, () =>
    dbg.selectors.getIsWaitingOnBreak(dbg.selectors.getCurrentThread())
  );

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    EventUtils.synthesizeMouseAtCenter(
      content.document.getElementsByTagName("video")[0],
      { type: "mouseover" },
      content
    );
  });

  await waitForPaused(dbg);
  await waitForSelectedSource(dbg, "videocontrols.js");

  await dbg.toolbox.closeToolbox();
});
