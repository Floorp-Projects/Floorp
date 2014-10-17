/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";


const URL = "http://mochi.test:8888/?rand=";
const PREF_BATTERY = "browser.sessionstore.interval_battery";
const PREF_CHARGING = "browser.sessionstore.interval";
const WAIT = 2000;
const LARGE_DURATION = 60000;
const SMALL_DURATION = 100;
let {Battery, Debugging} = Cu.import("resource://gre/modules/Battery.jsm", {});

/**
 * This test ensures that the session storage is saved at correct intervals
 * depending on the battery state
 */

function promiseWait(time) {
  return new Promise(resolve => {
    setTimeout(resolve, time);
  });
}


add_task(function* init() {
  Debugging.fake = true;
  Battery.charging = false;
  Battery.chargingTime = Infinity;
  Battery.dischargingTime = 50;

  Services.prefs.setIntPref(PREF_BATTERY, SMALL_DURATION);
  Services.prefs.setIntPref(PREF_CHARGING, LARGE_DURATION);

  registerCleanupFunction(function () {
    Services.prefs.clearUserPref(PREF_BATTERY);
    Services.prefs.clearUserPref(PREF_CHARGING);
    Debugging.fake = false;
  });
});

add_task(function* runtest() {
  let url = URL + Math.random();
  let tab = gBrowser.addTab(url);
  yield promiseWait(WAIT);
  let storage = JSON.parse(ss.getTabState(tab));
  is(storage.entries.length, 1, "sessionStorage correctly saved for non-charging battery state");
  let state = yield OS.File.read(SessionFile.Paths.recovery, { encoding: "utf-8" });
  ok(state.contains(url), "Sessionstore correctly saved to disk for non-charging battery state");
  gBrowser.removeTab(tab);

  Battery.charging = true;
  Battery.chargingTime = 100;
  Battery.dischargingTime = Infinity;
  Services.prefs.setIntPref(PREF_BATTERY, LARGE_DURATION);
  Services.prefs.setIntPref(PREF_CHARGING, SMALL_DURATION);

  url = URL + Math.random();
  tab = gBrowser.addTab(url);
  yield promiseWait(WAIT);
  storage = JSON.parse(ss.getTabState(tab));
  is(storage.entries.length, 1, "sessionStorage correctly saved for charging battery state");
  state = yield OS.File.read(SessionFile.Paths.recovery, { encoding: "utf-8" });
  ok(state.contains(url), "Sessionstore correctly saved to disk for charging battery state")
  gBrowser.removeTab(tab);
});
