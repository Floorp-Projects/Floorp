/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * Test that the profiler emits events when private browsing windows are opened
 * and closed.
 */
add_task(async function () {
  const {front, client} = await initPerfFront();

  is(await front.isLockedForPrivateBrowsing(), false,
    "The profiler is not locked for private browsing.");

  // Open up a new private browser window, and assert the correct events are fired.
  const profilerLocked = once(front, "profile-locked-by-private-browsing");
  const privateWindow = await BrowserTestUtils.openNewBrowserWindow({private: true});
  await profilerLocked;
  is(await front.isLockedForPrivateBrowsing(), true,
    "The profiler is now locked because of private browsing.");

  // Close the private browser window, and assert the correct events are fired.
  const profilerUnlocked = once(front, "profile-unlocked-from-private-browsing");
  await BrowserTestUtils.closeWindow(privateWindow);
  await profilerUnlocked;
  is(await front.isLockedForPrivateBrowsing(), false,
    "The profiler is available again after closing the private browsing window.");

  // Clean up.
  await front.destroy();
  await client.close();
});
