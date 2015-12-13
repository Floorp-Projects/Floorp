/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const CRASH_URL = "about:mozilla";
const CRASH_FAVICON = "chrome://branding/content/icon32.png";
const CRASH_SHENTRY = {url: CRASH_URL};
const CRASH_TAB = {entries: [CRASH_SHENTRY], image: CRASH_FAVICON};
const CRASH_STATE = {windows: [{tabs: [CRASH_TAB]}]};

const TAB_URL = "about:sessionrestore";
const TAB_FORMDATA = {url: TAB_URL, id: {sessionData: CRASH_STATE}};
const TAB_SHENTRY = {url: TAB_URL};
const TAB_STATE = {entries: [TAB_SHENTRY], formdata: TAB_FORMDATA};

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

  let view = browser.contentDocument.getElementById("tabList").view;
  ok(view.isContainer(0), "first entry is the window");
  is(view.getCellProperties(1, { id: "title" }), "icon",
    "second entry is the tab and has a favicon");

  browser.messageManager.loadFrameScript(FRAME_SCRIPT, true);

  // Wait until the new window was restored.
  let win = yield waitForNewWindow();
  yield BrowserTestUtils.closeWindow(win);

  let [{tabs: [{entries: [{url}]}]}] = JSON.parse(ss.getClosedWindowData());
  is(url, CRASH_URL, "session was restored correctly");
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
