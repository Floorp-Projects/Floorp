/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE_1 = "data:text/html,<html><body>A%20regular,%20everyday,%20normal%20page.";
const PAGE_2 = "data:text/html,<html><body>Another%20regular,%20everyday,%20normal%20page.";

// Turn off tab animations for testing
Services.prefs.setBoolPref("browser.tabs.animate", false);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("browser.tabs.animate");
});

// Allow tabs to restore on demand so we can test pending states
Services.prefs.clearUserPref("browser.sessionstore.restore_on_demand");

/**
 * Returns a Promise that resolves once a remote <xul:browser> has experienced
 * a crash. Also does the job of cleaning up the minidump of the crash.
 *
 * @param browser
 *        The <xul:browser> that will crash
 * @return Promise
 */
function crashBrowser(browser) {
  // This frame script is injected into the remote browser, and used to
  // intentionally crash the tab. We crash by using js-ctypes and dereferencing
  // a bad pointer. The crash should happen immediately upon loading this
  // frame script.
  let frame_script = () => {
    const Cu = Components.utils;
    Cu.import("resource://gre/modules/ctypes.jsm");

    let dies = function() {
      let zero = new ctypes.intptr_t(8);
      let badptr = ctypes.cast(zero, ctypes.PointerType(ctypes.int32_t));
      badptr.contents
    };

    dump("Et tu, Brute?");
    dies();
  }

  let crashCleanupPromise = new Promise((resolve, reject) => {
    let observer = (subject, topic, data) => {
      is(topic, 'ipc:content-shutdown', 'Received correct observer topic.');
      ok(subject instanceof Ci.nsIPropertyBag2,
         'Subject implements nsIPropertyBag2.');
      // we might see this called as the process terminates due to previous tests.
      // We are only looking for "abnormal" exits...
      if (!subject.hasKey("abnormal")) {
        info("This is a normal termination and isn't the one we are looking for...");
        return;
      }

      let dumpID;
      if ('nsICrashReporter' in Ci) {
        dumpID = subject.getPropertyAsAString('dumpID');
        ok(dumpID, "dumpID is present and not an empty string");
      }

      if (dumpID) {
        let minidumpDirectory = getMinidumpDirectory();
        removeFile(minidumpDirectory, dumpID + '.dmp');
        removeFile(minidumpDirectory, dumpID + '.extra');
      }

      Services.obs.removeObserver(observer, 'ipc:content-shutdown');
      info("Crash cleaned up");
      resolve();
    };

    Services.obs.addObserver(observer, 'ipc:content-shutdown');
  });

  let aboutTabCrashedLoadPromise = new Promise((resolve, reject) => {
    browser.addEventListener("AboutTabCrashedLoad", function onCrash() {
      browser.removeEventListener("AboutTabCrashedLoad", onCrash, false);
      info("about:tabcrashed loaded");
      resolve();
    }, false, true);
  });

  // This frame script will crash the remote browser as soon as it is
  // evaluated.
  let mm = browser.messageManager;
  mm.loadFrameScript("data:,(" + frame_script.toString() + ")();", false);
  return Promise.all([crashCleanupPromise, aboutTabCrashedLoadPromise]).then(() => {
    let tab = gBrowser.getTabForBrowser(browser);
    is(tab.getAttribute("crashed"), "true", "Tab should be marked as crashed");
  });
}

function clickButton(browser, id) {
  info("Clicking " + id);

  let frame_script = (id) => {
    let button = content.document.getElementById(id);
    button.click();
  };

  let mm = browser.messageManager;
  mm.loadFrameScript("data:,(" + frame_script.toString() + ")('" + id + "');", false);
}

/**
 * Returns the directory where crash dumps are stored.
 *
 * @return nsIFile
 */
function getMinidumpDirectory() {
  let dir = Services.dirsvc.get('ProfD', Ci.nsIFile);
  dir.append("minidumps");
  return dir;
}

/**
 * Removes a file from a directory. This is a no-op if the file does not
 * exist.
 *
 * @param directory
 *        The nsIFile representing the directory to remove from.
 * @param filename
 *        A string for the file to remove from the directory.
 */
function removeFile(directory, filename) {
  let file = directory.clone();
  file.append(filename);
  if (file.exists()) {
    file.remove(false);
  }
}

/**
 * Checks the documentURI of the root document of a remote browser
 * to see if it equals URI. Returns a Promise that resolves if
 * there is a match, and rejects with an error message if they
 * do not match.
 *
 * @param browser
 *        The remote <xul:browser> to check the root document URI in.
 * @param URI
 *        A string to match the root document URI against.
 * @return Promise
 */
function promiseContentDocumentURIEquals(browser, URI) {
  return new Promise((resolve, reject) => {
    let frame_script = () => {
      sendAsyncMessage("test:documenturi", {
        uri: content.document.documentURI,
      });
    };

    let mm = browser.messageManager;
    mm.addMessageListener("test:documenturi", function onMessage(message) {
      mm.removeMessageListener("test:documenturi", onMessage);
      let contentURI = message.data.uri;
      if (contentURI == URI) {
        resolve();
      } else {
        reject(`Content has URI ${contentURI} which does not match ${URI}`);
      }
    });

    mm.loadFrameScript("data:,(" + frame_script.toString() + ")();", false);
  });
}

/**
 * Checks the window.history.length of the root window of a remote
 * browser to see if it equals length. Returns a Promise that resolves
 * if there is a match, and rejects with an error message if they
 * do not match.
 *
 * @param browser
 *        The remote <xul:browser> to check the root window.history.length
 * @param length
 *        The expected history length
 * @return Promise
 */
function promiseHistoryLength(browser, length) {
  return new Promise((resolve, reject) => {
    let frame_script = () => {
      sendAsyncMessage("test:historylength", {
        length: content.history.length,
      });
    };

    let mm = browser.messageManager;
    mm.addMessageListener("test:historylength", function onMessage(message) {
      mm.removeMessageListener("test:historylength", onMessage);
      let contentLength = message.data.length;
      if (contentLength == length) {
        resolve();
      } else {
        reject(`Content has window.history.length ${contentLength} which does ` +
               `not equal expected ${length}`);
      }
    });

    mm.loadFrameScript("data:,(" + frame_script.toString() + ")();", false);
  });
}

/**
 * Checks that if a tab crashes, that information about the tab crashed
 * page does not get added to the tab history.
 */
add_task(function test_crash_page_not_in_history() {
  let newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  let browser = newTab.linkedBrowser;
  ok(browser.isRemoteBrowser, "Should be a remote browser");
  yield promiseBrowserLoaded(browser);

  browser.loadURI(PAGE_1);
  yield promiseBrowserLoaded(browser);
  TabState.flush(browser);

  // Crash the tab
  yield crashBrowser(browser);

  // Check the tab state and make sure the tab crashed page isn't
  // mentioned.
  let {entries} = JSON.parse(ss.getTabState(newTab));
  is(entries.length, 1, "Should have a single history entry");
  is(entries[0].url, PAGE_1,
    "Single entry should be the page we visited before crashing");

  gBrowser.removeTab(newTab);
});

/**
 * Checks that if a tab crashes, that when we browse away from that page
 * to a non-blacklisted site (so the browser becomes remote again), that
 * we record history for that new visit.
 */
add_task(function test_revived_history_from_remote() {
  let newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  let browser = newTab.linkedBrowser;
  ok(browser.isRemoteBrowser, "Should be a remote browser");
  yield promiseBrowserLoaded(browser);

  browser.loadURI(PAGE_1);
  yield promiseBrowserLoaded(browser);
  TabState.flush(browser);

  // Crash the tab
  yield crashBrowser(browser);

  // Browse to a new site that will cause the browser to
  // become remote again.
  browser.loadURI(PAGE_2);
  yield promiseTabRestored(newTab);
  ok(!newTab.hasAttribute("crashed"), "Tab shouldn't be marked as crashed anymore.");
  ok(browser.isRemoteBrowser, "Should be a remote browser");
  TabState.flush(browser);

  // Check the tab state and make sure the tab crashed page isn't
  // mentioned.
  let {entries} = JSON.parse(ss.getTabState(newTab));
  is(entries.length, 2, "Should have two history entries");
  is(entries[0].url, PAGE_1,
    "First entry should be the page we visited before crashing");
  is(entries[1].url, PAGE_2,
    "Second entry should be the page we visited after crashing");

  gBrowser.removeTab(newTab);
});

/**
 * Checks that if a tab crashes, that when we browse away from that page
 * to a blacklisted site (so the browser stays non-remote), that
 * we record history for that new visit.
 */
add_task(function test_revived_history_from_non_remote() {
  let newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  let browser = newTab.linkedBrowser;
  ok(browser.isRemoteBrowser, "Should be a remote browser");
  yield promiseBrowserLoaded(browser);

  browser.loadURI(PAGE_1);
  yield promiseBrowserLoaded(browser);
  TabState.flush(browser);

  // Crash the tab
  yield crashBrowser(browser);

  // Browse to a new site that will not cause the browser to
  // become remote again.
  browser.loadURI("about:mozilla");
  yield promiseBrowserLoaded(browser);
  ok(!newTab.hasAttribute("crashed"), "Tab shouldn't be marked as crashed anymore.");
  ok(!browser.isRemoteBrowser, "Should not be a remote browser");
  TabState.flush(browser);

  // Check the tab state and make sure the tab crashed page isn't
  // mentioned.
  let {entries} = JSON.parse(ss.getTabState(newTab));
  is(entries.length, 2, "Should have two history entries");
  is(entries[0].url, PAGE_1,
    "First entry should be the page we visited before crashing");
  is(entries[1].url, "about:mozilla",
    "Second entry should be the page we visited after crashing");

  gBrowser.removeTab(newTab);
});

/**
 * Checks that we can revive a crashed tab back to the page that
 * it was on when it crashed.
 */
add_task(function test_revive_tab_from_session_store() {
  let newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  let browser = newTab.linkedBrowser;
  ok(browser.isRemoteBrowser, "Should be a remote browser");
  yield promiseBrowserLoaded(browser);

  browser.loadURI(PAGE_1);
  yield promiseBrowserLoaded(browser);

  let newTab2 = gBrowser.addTab();
  let browser2 = newTab2.linkedBrowser;
  ok(browser2.isRemoteBrowser, "Should be a remote browser");
  yield promiseBrowserLoaded(browser2);

  browser.loadURI(PAGE_1);
  yield promiseBrowserLoaded(browser);

  browser.loadURI(PAGE_2);
  yield promiseBrowserLoaded(browser);

  TabState.flush(browser);

  // Crash the tab
  yield crashBrowser(browser);
  is(newTab2.getAttribute("crashed"), "true", "Second tab should be crashed too.");

  // Use SessionStore to revive the tab
  clickButton(browser, "restoreTab");
  yield promiseTabRestored(newTab);
  ok(!newTab.hasAttribute("crashed"), "Tab shouldn't be marked as crashed anymore.");
  is(newTab2.getAttribute("crashed"), "true", "Second tab should still be crashed though.");

  // We can't just check browser.currentURI.spec, because from
  // the outside, a crashed tab has the same URI as the page
  // it crashed on (much like an about:neterror page). Instead,
  // we have to use the documentURI on the content.
  yield promiseContentDocumentURIEquals(browser, PAGE_2);

  // We should also have two entries in the browser history.
  yield promiseHistoryLength(browser, 2);

  gBrowser.removeTab(newTab);
  gBrowser.removeTab(newTab2);
});

/**
 * Checks that we can revive a crashed tab back to the page that
 * it was on when it crashed.
 */
add_task(function test_revive_all_tabs_from_session_store() {
  let newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  let browser = newTab.linkedBrowser;
  ok(browser.isRemoteBrowser, "Should be a remote browser");
  yield promiseBrowserLoaded(browser);

  browser.loadURI(PAGE_1);
  yield promiseBrowserLoaded(browser);

  let newTab2 = gBrowser.addTab(PAGE_1);
  let browser2 = newTab2.linkedBrowser;
  ok(browser2.isRemoteBrowser, "Should be a remote browser");
  yield promiseBrowserLoaded(browser2);

  browser.loadURI(PAGE_1);
  yield promiseBrowserLoaded(browser);

  browser.loadURI(PAGE_2);
  yield promiseBrowserLoaded(browser);

  TabState.flush(browser);
  TabState.flush(browser2);

  // Crash the tab
  yield crashBrowser(browser);
  is(newTab2.getAttribute("crashed"), "true", "Second tab should be crashed too.");

  // Use SessionStore to revive all the tabs
  clickButton(browser, "restoreAll");
  yield promiseTabRestored(newTab);
  ok(!newTab.hasAttribute("crashed"), "Tab shouldn't be marked as crashed anymore.");
  ok(!newTab.hasAttribute("pending"), "Tab shouldn't be pending.");
  ok(!newTab2.hasAttribute("crashed"), "Second tab shouldn't be marked as crashed anymore.");
  ok(newTab2.hasAttribute("pending"), "Second tab should be pending.");

  gBrowser.selectedTab = newTab2;
  yield promiseTabRestored(newTab2);
  ok(!newTab2.hasAttribute("pending"), "Second tab shouldn't be pending.");

  // We can't just check browser.currentURI.spec, because from
  // the outside, a crashed tab has the same URI as the page
  // it crashed on (much like an about:neterror page). Instead,
  // we have to use the documentURI on the content.
  yield promiseContentDocumentURIEquals(browser, PAGE_2);
  yield promiseContentDocumentURIEquals(browser2, PAGE_1);

  // We should also have two entries in the browser history.
  yield promiseHistoryLength(browser, 2);

  gBrowser.removeTab(newTab);
  gBrowser.removeTab(newTab2);
});

/**
 * Checks that about:tabcrashed can close the current tab
 */
add_task(function test_close_tab_after_crash() {
  let newTab = gBrowser.addTab();
  gBrowser.selectedTab = newTab;
  let browser = newTab.linkedBrowser;
  ok(browser.isRemoteBrowser, "Should be a remote browser");
  yield promiseBrowserLoaded(browser);

  browser.loadURI(PAGE_1);
  yield promiseBrowserLoaded(browser);

  TabState.flush(browser);

  // Crash the tab
  yield crashBrowser(browser);

  let promise = promiseEvent(gBrowser.tabContainer, "TabClose");

  // Click the close tab button
  clickButton(browser, "closeTab");
  yield promise;

  is(gBrowser.tabs.length, 1, "Should have closed the tab");
});
