/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

waitForExplicitFinish();

var kUrlSource = "http://mochi.test:8888/";
var kDataSource = "data:text/html,";

var gOldPref;
var gWin, gWin1, gWin2;
var gTab, gTab1, gTab2;
var gLock, gLock1, gLock2;
var gCurStepIndex = -1;
var gSteps = [
  function basicWakeLock() {
    gTab = BrowserTestUtils.addTab(gBrowser, kUrlSource);
    gWin = gBrowser.getBrowserForTab(gTab).contentWindow;
    let browser = gBrowser.getBrowserForTab(gTab);

    browser.addEventListener("load", function(e) {
      let nav = gWin.navigator;
      let power = nav.mozPower;
      gLock = nav.requestWakeLock("test");

      ok(gLock != null,
         "navigator.requestWakeLock should return a wake lock");
      is(gLock.topic, "test",
         "wake lock should remember the locked topic");
      isnot(power.getWakeLockState("test"), "unlocked",
            "topic is locked");

      gLock.unlock();

      is(gLock.topic, "test",
         "wake lock should remember the locked topic even after unlock");
      is(power.getWakeLockState("test"), "unlocked",
         "topic is unlocked");

      try {
        gLock.unlock();
        ok(false, "Should have thrown an error.");
      } catch (e) {
        is(e.name, "InvalidStateError", "double unlock should throw InvalidStateError");
        is(e.code, DOMException.INVALID_STATE_ERR, "double unlock should throw InvalidStateError");
      }

      gBrowser.removeTab(gTab);

      executeSoon(runNextStep);
    }, {capture: true, once: true});
  },
  function multiWakeLock() {
    gTab = BrowserTestUtils.addTab(gBrowser, kUrlSource);
    gWin = gBrowser.getBrowserForTab(gTab).contentWindow;
    let browser = gBrowser.getBrowserForTab(gTab);

    browser.addEventListener("load", function(e) {
      let nav = gWin.navigator;
      let power = nav.mozPower;
      let count = 0;
      power.addWakeLockListener(function onWakeLockEvent(topic, state) {
        is(topic, "test", "gLock topic is test");
        ok(state == "unlocked" ||
           state == "locked-foreground" ||
           state == "locked-background",
           "wake lock should be either locked or unlocked");
        count++;
        if (state == "locked-foreground" ||
            state == "locked-background") {
          is(count, 1,
             "wake lock should be locked and the listener should only fire once");
        }
        if (state == "unlocked") {
          is(count, 2,
             "wake lock should be unlocked and the listener should only fire once");

          ok(power.getWakeLockState("test") == "unlocked",
             "topic is unlocked");
          power.removeWakeLockListener(onWakeLockEvent);
          gBrowser.removeTab(gTab);
          executeSoon(runNextStep);
        }
      });

      gLock1 = nav.requestWakeLock("test");
      isnot(power.getWakeLockState("test"), "unlocked",
            "topic is locked");

      gLock2 = nav.requestWakeLock("test");
      isnot(power.getWakeLockState("test"), "unlocked",
            "topic is locked");

      gLock1.unlock();
      isnot(power.getWakeLockState("test"), "unlocked",
            "topic is locked");

      gLock2.unlock();
    }, {capture: true, once: true});
  },
  function crossTabWakeLock1() {
    gTab1 = BrowserTestUtils.addTab(gBrowser, kUrlSource);
    gWin1 = gBrowser.getBrowserForTab(gTab1).contentWindow;
    gTab2 = BrowserTestUtils.addTab(gBrowser, kUrlSource);
    gWin2 = gBrowser.getBrowserForTab(gTab2).contentWindow;

    gBrowser.selectedTab = gTab1;
    let browser = gBrowser.getBrowserForTab(gTab2);

    browser.addEventListener("load", function(e) {
      gLock2 = gWin2.navigator.requestWakeLock("test");
      is(gWin2.document.hidden, true,
         "window is background")
      is(gWin2.navigator.mozPower.getWakeLockState("test"), "locked-background",
         "wake lock is background");
      let doc2 = gWin2.document;
      doc2.addEventListener("visibilitychange", function onVisibilityChange(e) {
        if (!doc2.hidden) {
          doc2.removeEventListener("visibilitychange", onVisibilityChange);
          executeSoon(runNextStep);
        }
      });
      gBrowser.selectedTab = gTab2;
    }, {capture: true, once: true});
  },
  function crossTabWakeLock2() {
    is(gWin2.document.hidden, false,
       "window is foreground")
    is(gWin2.navigator.mozPower.getWakeLockState("test"), "locked-foreground",
      "wake lock is foreground");
    gWin2.addEventListener("pagehide", function(e) {
      executeSoon(runNextStep);
    }, {capture: true, once: true});
    gWin2.addEventListener("pageshow", function(e) {
      executeSoon(runNextStep);
    }, {capture: true, once: true});
    gWin2.location = kDataSource;
  },
  function crossTabWakeLock3() {
    is(gWin1.navigator.mozPower.getWakeLockState("test"), "unlocked",
       "wake lock should auto-unlock when page is unloaded");
    gWin2.back();
    // runNextStep called in onPageShow
  },
  function crossTabWakeLock4() {
    is(gWin1.navigator.mozPower.getWakeLockState("test"), "locked-foreground",
       "wake lock should auto-reacquire when page is available again");
    gBrowser.selectedTab = gTab1;
    executeSoon(runNextStep);
  },
  function crossTabWakeLock5() {
    // Test again in background tab
    is(gWin2.document.hidden, true,
       "window is background")
    is(gWin2.navigator.mozPower.getWakeLockState("test"), "locked-background",
      "wake lock is background");
    gWin2.addEventListener("pagehide", function(e) {
      executeSoon(runNextStep);
    }, {capture: true, once: true});
    gWin2.addEventListener("pageshow", function(e) {
      executeSoon(runNextStep);
    }, {capture: true, once: true});
    gWin2.location = kDataSource;
  },
  function crossTabWakeLock6() {
    is(gWin1.navigator.mozPower.getWakeLockState("test"), "unlocked",
       "wake lock should auto-unlock when page is unloaded");
    gWin2.back();
    // runNextStep called in onPageShow
  },
  function crossTabWakeLock7() {
    is(gWin1.navigator.mozPower.getWakeLockState("test"), "locked-background",
       "wake lock should auto-reacquire when page is available again");
    gLock2.unlock();
    gBrowser.selectedTab = gTab2;
    executeSoon(runNextStep);
  },
  function crossTabWakeLock8() {
    is(gWin1.document.hidden, true,
       "gWin1 is background");
    is(gWin2.document.hidden, false,
       "gWin2 is foreground");

    gLock1 = gWin1.navigator.requestWakeLock("test");
    gLock2 = gWin2.navigator.requestWakeLock("test");

    is(gWin1.navigator.mozPower.getWakeLockState("test"), "locked-foreground",
       "topic is locked-foreground when one page is foreground and one is background");

    gLock2.unlock();

    is(gWin1.navigator.mozPower.getWakeLockState("test"), "locked-background",
       "topic is locked-background when all locks are background");

    gLock2 = gWin2.navigator.requestWakeLock("test");

    is(gWin1.navigator.mozPower.getWakeLockState("test"), "locked-foreground",
       "topic is locked-foreground when one page is foreground and one is background");

    gLock1.unlock();

    is(gWin1.navigator.mozPower.getWakeLockState("test"), "locked-foreground",
       "topic is locked-foreground");

    gBrowser.removeTab(gTab1);
    gBrowser.removeTab(gTab2);
    executeSoon(runNextStep);
  },
];

function runNextStep() {
  gCurStepIndex++;
  if (gCurStepIndex < gSteps.length) {
    gSteps[gCurStepIndex]();
  } else {
    finish();
  }
}

function test() {
  SpecialPowers.pushPrefEnv({"set": [["dom.wakelock.enabled", true]]},
                            runNextStep);
}
