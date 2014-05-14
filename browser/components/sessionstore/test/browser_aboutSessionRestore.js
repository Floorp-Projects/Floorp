/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CRASH_SHENTRY = {url: "about:mozilla"};
const CRASH_TAB = {entries: [CRASH_SHENTRY]};
const CRASH_STATE = {windows: [{tabs: [CRASH_TAB]}]};

const TAB_FORMDATA = {id: {sessionData: CRASH_STATE}};
const TAB_SHENTRY = {url: "about:sessionrestore", formdata: TAB_FORMDATA};
const TAB_STATE = {entries: [TAB_SHENTRY]};

const FRAME_SCRIPT = "data:," +
                     "content.document.getElementById('errorTryAgain').click()";

add_task(function* () {
  // Prepare a blank tab.
  let tab = gBrowser.addTab("about:blank");
  let browser = tab.linkedBrowser;
  yield promiseBrowserLoaded(browser);

  // Fake a post-crash tab.
  ss.setTabState(tab, JSON.stringify(TAB_STATE));
  yield promiseTabRestored(tab);

  ok(gBrowser.tabs.length > 1, "we have more than one tab");
  browser.messageManager.loadFrameScript(FRAME_SCRIPT, true);

  // Wait until the new window was restored.
  let win = yield waitForNewWindow();
  yield promiseWindowClosed(win);

  let [{tabs: [{entries: [{url}]}]}] = JSON.parse(ss.getClosedWindowData());
  is(url, "about:mozilla", "session was restored correctly");
  ss.forgetClosedWindow(0);
});

function waitForNewWindow() {
  return new Promise(resolve => {
    Services.obs.addObserver(function observe(win, topic) {
      Services.obs.removeObserver(observe, topic);
      resolve(win);
    }, "browser-delayed-startup-finished", false);
  });
}
