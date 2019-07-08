/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

add_task(async () => {
  info("creating test window");
  let win = await BrowserTestUtils.openNewBrowserWindow();

  const pm = Cc["@mozilla.org/power/powermanagerservice;1"].getService(
    Ci.nsIPowerManagerService
  );

  is(
    pm.getWakeLockState("screen"),
    "unlocked",
    "Wakelock should be unlocked state"
  );

  info("aquiring wakelock");
  const wakelock = pm.newWakeLock("screen", win);
  isnot(
    pm.getWakeLockState("screen"),
    "unlocked",
    "Wakelock shouldn't be unlocked state"
  );

  info("releasing wakelock");
  wakelock.unlock();
  is(
    pm.getWakeLockState("screen"),
    "unlocked",
    "Wakelock should be unlocked state"
  );

  info("closing test window");
  await BrowserTestUtils.closeWindow(win);
});
