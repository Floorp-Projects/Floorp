/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URL = "http://mochi.test:8888/browser/browser/components/" +
                 "sessionstore/test/browser_637020_slow.sjs";

const TEST_STATE = {
  windows: [{
    tabs: [
      { entries: [{ url: "about:mozilla" }] },
      { entries: [{ url: "about:robots" }] }
    ]
  }, {
    tabs: [
      { entries: [{ url: TEST_URL }] },
      { entries: [{ url: TEST_URL }] }
    ]
  }]
};

function test() {
  TestRunner.run();
}

/**
 * This test ensures that windows that have just been restored will be marked
 * as dirty, otherwise _getCurrentState() might ignore them when collecting
 * state for the first time and we'd just save them as empty objects.
 *
 * The dirty state acts as a cache to not collect data from all windows all the
 * time, so at the beginning, each window must be dirty so that we collect
 * their state at least once.
 */

function runTests() {
  let win;

  // Wait until the new window has been opened.
  Services.obs.addObserver(function onOpened(subject) {
    Services.obs.removeObserver(onOpened, "domwindowopened");
    win = subject;
    executeSoon(next);
  }, "domwindowopened", false);

  // Set the new browser state that will
  // restore a window with two slowly loading tabs.
  yield SessionStore.setBrowserState(JSON.stringify(TEST_STATE));

  // The window has now been opened. Check the state that is returned,
  // this should come from the cache while the window isn't restored, yet.
  info("the window has been opened");
  checkWindows();

  // The history has now been restored and the tabs are loading. The data must
  // now come from the window, if it's correctly been marked as dirty before.
  yield whenDelayedStartupFinished(win, next);
  info("the delayed startup has finished");
  checkWindows();
}

function checkWindows() {
  let state = JSON.parse(SessionStore.getBrowserState());
  is(state.windows[0].tabs.length, 2, "first window has two tabs");
  is(state.windows[1].tabs.length, 2, "second window has two tabs");
}
