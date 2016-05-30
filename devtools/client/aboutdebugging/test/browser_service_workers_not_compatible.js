/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that Service Worker section should show warning message in
// about:debugging if any of following conditions is met:
// 1. service worker is disabled
// 2. the about:debugging pannel is openned in private browsing mode
// 3. the about:debugging pannel is openned in private content window

var imgClass = ".service-worker-disabled .warning";

add_task(function* () {
  yield new Promise(done => {
    info("disable service workers");
    let options = {"set": [
      ["dom.serviceWorkers.enabled", false],
    ]};
    SpecialPowers.pushPrefEnv(options, done);
  });

  let { tab, document } = yield openAboutDebugging("workers");
  // Check that the warning img appears in the UI
  let img = document.querySelector(imgClass);
  ok(img, "warning message is rendered");

  yield closeAboutDebugging(tab);
});

add_task(function* () {
  yield new Promise(done => {
    info("set private browsing mode as default");
    let options = {"set": [
      ["browser.privatebrowsing.autostart", true],
    ]};
    SpecialPowers.pushPrefEnv(options, done);
  });

  let { tab, document } = yield openAboutDebugging("workers");
  // Check that the warning img appears in the UI
  let img = document.querySelector(imgClass);
  ok(img, "warning message is rendered");

  yield closeAboutDebugging(tab);
});

add_task(function* () {
  info("Opening a new private window");
  let win = OpenBrowserWindow({private: true});
  yield waitForDelayedStartupFinished(win);

  let { tab, document } = yield openAboutDebugging("workers", win);
  // Check that the warning img appears in the UI
  let img = document.querySelector(imgClass);
  ok(img, "warning message is rendered");

  yield closeAboutDebugging(tab, win);
  win.close();
});
