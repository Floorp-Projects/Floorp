/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

var tab0, tab1, tab2;
var testStep = -1;

var expected = [];
function expect(notification, win) {
  expected.push({ notification: notification, window: win });
}

function notification(win, topic) {
  if (expected.length == 0) {
    is(topic, null, "Shouldn't see a notification");
    return;
  }

  let { notification, window } = expected.shift();
  if (Cu.isDeadWrapper(window)) {
    // Sometimes we end up with a nuked window reference here :-(
    return;
  }
  is(topic, notification, "Saw the expected notification");
  is(win, window, "Saw the expected window");
}

function after(notification, callback) {
  function observer() {
    Services.obs.removeObserver(observer, notification);
    executeSoon(callback);
  }
  Services.obs.addObserver(observer, notification, false);
}

function test() {
  if (!isTiltEnabled()) {
    aborting();
    info("Skipping tab switch test because Tilt isn't enabled.");
    return;
  }
  if (!isWebGLSupported()) {
    aborting();
    info("Skipping tab switch test because WebGL isn't supported.");
    return;
  }

  Services.obs.addObserver(notification, STARTUP, false);
  Services.obs.addObserver(notification, INITIALIZING, false);
  Services.obs.addObserver(notification, INITIALIZED, false);
  Services.obs.addObserver(notification, DESTROYING, false);
  Services.obs.addObserver(notification, BEFORE_DESTROYED, false);
  Services.obs.addObserver(notification, DESTROYED, false);
  Services.obs.addObserver(notification, SHOWN, false);
  Services.obs.addObserver(notification, HIDDEN, false);

  waitForExplicitFinish();

  tab0 = gBrowser.selectedTab;
  nextStep();
}

function createTab2() {
}

var testSteps = [
  function step0() {
    tab1 = createTab(function() {
      expect(STARTUP, tab1.linkedBrowser.contentWindow);
      expect(INITIALIZING, tab1.linkedBrowser.contentWindow);
      expect(INITIALIZED, tab1.linkedBrowser.contentWindow);
      after(INITIALIZED, nextStep);

      createTilt({}, false, function suddenDeath()
      {
        ok(false, "Tilt could not be initialized properly.");
        cleanup();
      });
    });
  },
  function step1() {
    expect(HIDDEN, tab1.linkedBrowser.contentWindow);

    tab2 = createTab(function() {
      expect(STARTUP, tab2.linkedBrowser.contentWindow);
      expect(INITIALIZING, tab2.linkedBrowser.contentWindow);
      expect(INITIALIZED, tab2.linkedBrowser.contentWindow);
      after(INITIALIZED, nextStep);

      createTilt({}, false, function suddenDeath()
      {
        ok(false, "Tilt could not be initialized properly.");
        cleanup();
      });
    });
  },
  function step2() {
    expect(HIDDEN, tab2.linkedBrowser.contentWindow);
    after(HIDDEN, nextStep);

    gBrowser.selectedTab = tab0;
  },
  function step3() {
    expect(SHOWN, tab2.linkedBrowser.contentWindow);
    after(SHOWN, nextStep);

    gBrowser.selectedTab = tab2;
  },
  function step4() {
    expect(HIDDEN, tab2.linkedBrowser.contentWindow);
    expect(SHOWN, tab1.linkedBrowser.contentWindow);
    after(SHOWN, nextStep);

    gBrowser.selectedTab = tab1;
  },
  function step5() {
    expect(HIDDEN, tab1.linkedBrowser.contentWindow);
    expect(SHOWN, tab2.linkedBrowser.contentWindow);
    after(SHOWN, nextStep);

    gBrowser.selectedTab = tab2;
  },
  function step6() {
    expect(DESTROYING, tab2.linkedBrowser.contentWindow);
    expect(BEFORE_DESTROYED, tab2.linkedBrowser.contentWindow);
    expect(DESTROYED, tab2.linkedBrowser.contentWindow);
    after(DESTROYED, nextStep);

    Tilt.destroy(Tilt.currentWindowId, true);
  },
  function step7() {
    expect(SHOWN, tab1.linkedBrowser.contentWindow);

    gBrowser.removeCurrentTab();
    tab2 = null;

    expect(DESTROYING, tab1.linkedBrowser.contentWindow);
    expect(HIDDEN, tab1.linkedBrowser.contentWindow);
    expect(BEFORE_DESTROYED, tab1.linkedBrowser.contentWindow);
    expect(DESTROYED, tab1.linkedBrowser.contentWindow);
    after(DESTROYED, nextStep);

    gBrowser.removeCurrentTab();
    tab1 = null;
  },
  function step8_cleanup() {
    is(gBrowser.selectedTab, tab0, "Should be back to the first tab");

    cleanup();
  }
];

function cleanup() {
  if (tab1) {
    gBrowser.removeTab(tab1);
    tab1 = null;
  }
  if (tab2) {
    gBrowser.removeTab(tab2);
    tab2 = null;
  }

  Services.obs.removeObserver(notification, STARTUP);
  Services.obs.removeObserver(notification, INITIALIZING);
  Services.obs.removeObserver(notification, INITIALIZED);
  Services.obs.removeObserver(notification, DESTROYING);
  Services.obs.removeObserver(notification, BEFORE_DESTROYED);
  Services.obs.removeObserver(notification, DESTROYED);
  Services.obs.removeObserver(notification, SHOWN);
  Services.obs.removeObserver(notification, HIDDEN);

  finish();
}

function nextStep() {
  let step = testSteps.shift();
  info("Executing " + step.name);
  step();
}
