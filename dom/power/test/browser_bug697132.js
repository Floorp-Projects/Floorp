"use strict";

waitForExplicitFinish();

let kPrefNode = "dom.power.whitelist";
let kPageSource1 = "data:text/html,1";
let kPageSource2 = "data:text/html,2";

let gOldPref;
let gWin, gWin1, gWin2;
let gTab, gTab1, gTab2;
let gLock, gLock1, gLock2;
let gCurStepIndex = -1;
let gSteps = [
  function basicWakeLock() {
    gTab = gBrowser.addTab(kPageSource1);
    gWin = gBrowser.getBrowserForTab(gTab).contentWindow;
    let browser = gBrowser.getBrowserForTab(gTab);

    browser.addEventListener("load", function onLoad(e) {
      browser.removeEventListener("load", onLoad, true);
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
        is(e.code, DOMException.INVALID_STATE_ERR, "double unlock should throw InvalidStateError");
      }

      gBrowser.removeTab(gTab);

      executeSoon(runNextStep);
    }, true);
  },
  function multiWakeLock() {
    gTab = gBrowser.addTab(kPageSource1);
    gWin = gBrowser.getBrowserForTab(gTab).contentWindow;
    let browser = gBrowser.getBrowserForTab(gTab);

    browser.addEventListener("load", function onLoad(e) {
      browser.removeEventListener("load", onLoad, true);
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
    }, true);
  },
  function crossTabWakeLock1() {
    gTab1 = gBrowser.addTab(kPageSource1);
    gWin1 = gBrowser.getBrowserForTab(gTab1).contentWindow;
    gTab2 = gBrowser.addTab(kPageSource1);
    gWin2 = gBrowser.getBrowserForTab(gTab2).contentWindow;

    gBrowser.selectedTab = gTab1;
    let browser = gBrowser.getBrowserForTab(gTab2);

    browser.addEventListener("load", function onLoad(e) {
      browser.removeEventListener("load", onLoad, true);
      gLock2 = gWin2.navigator.requestWakeLock("test");
      is(gWin2.document.mozHidden, true,
         "window is background")
      is(gWin2.navigator.mozPower.getWakeLockState("test"), "locked-background",
         "wake lock is background");
      let doc2 = gWin2.document;
      doc2.addEventListener("mozvisibilitychange", function onVisibilityChange(e) {
        if (!doc2.mozHidden) {
          doc2.removeEventListener("mozvisibilitychange", onVisibilityChange);
          executeSoon(runNextStep);
        }
      });
      gBrowser.selectedTab = gTab2;
    }, true);
  },
  function crossTabWakeLock2() {
    is(gWin2.document.mozHidden, false,
       "window is foreground")
    is(gWin2.navigator.mozPower.getWakeLockState("test"), "locked-foreground",
      "wake lock is foreground");
    gWin2.addEventListener("pagehide", function onPageHide(e) {
      gWin2.removeEventListener("pagehide", onPageHide, true);
      executeSoon(runNextStep);
    }, true);
    gWin2.addEventListener("pageshow", function onPageShow(e) {
      gWin2.removeEventListener("pageshow", onPageShow, true);
      executeSoon(runNextStep);
    }, true);
    gWin2.location = kPageSource2;
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
    is(gWin2.document.mozHidden, true,
       "window is background")
    is(gWin2.navigator.mozPower.getWakeLockState("test"), "locked-background",
      "wake lock is background");
    gWin2.addEventListener("pagehide", function onPageHide(e) {
      gWin2.removeEventListener("pagehide", onPageHide, true);
      executeSoon(runNextStep);
    }, true);
    gWin2.addEventListener("pageshow", function onPageShow(e) {
      gWin2.removeEventListener("pageshow", onPageShow, true);
      executeSoon(runNextStep);
    }, true);
    gWin2.location = kPageSource2;
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
    is(gWin1.document.mozHidden, true,
       "gWin1 is background");
    is(gWin2.document.mozHidden, false,
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
    Services.prefs.setCharPref(kPrefNode, gOldPref);
    finish();
  }
}

function test() {
  try {
    gOldPref = Services.prefs.getCharPref(kPrefNode);
  } catch (e) {
    gOldPref = "";
  }
  // data url inherits its parent's principal, which is |about:| here.
  Services.prefs.setCharPref(kPrefNode, "about:");
  runNextStep();
}
