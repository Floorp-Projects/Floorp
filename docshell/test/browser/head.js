/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* Helper function for timeline tests.  Returns an async task that is
 * suitable for use as a particular timeline test.
 * @param string frameScriptName
 *        Base name of the frame script file.
 * @param string url
 *        URL to load.
 */
function makeTimelineTest(frameScriptName, url) {
  info("in timelineTest");
  return Task.async(function*() {
    info("in in timelineTest");
    waitForExplicitFinish();

    yield timelineTestOpenUrl(url);

    const here = "chrome://mochitests/content/browser/docshell/test/browser/";

    let mm = gBrowser.selectedBrowser.messageManager;
    mm.loadFrameScript(here + "frame-head.js", false);
    mm.loadFrameScript(here + frameScriptName, false);

    // Set up some listeners so that timeline tests running in the
    // content process can forward their results to the main process.
    mm.addMessageListener("browser:test:ok", function(message) {
      ok(message.data.value, message.data.message);
    });
    mm.addMessageListener("browser:test:info", function(message) {
      info(message.data.message);
    });
    mm.addMessageListener("browser:test:finish", function(ignore) {
      gBrowser.removeCurrentTab();
      finish();
    });
  });
}

/* Open a URL for a timeline test.  */
function timelineTestOpenUrl(url) {
  window.focus();

  let tabSwitchPromise = new Promise((resolve, reject) => {
    window.gBrowser.addEventListener("TabSwitchDone", function listener() {
      window.gBrowser.removeEventListener("TabSwitchDone", listener);
      resolve();
    });
  });

  let loadPromise = new Promise(function(resolve, reject) {
    let tab = window.gBrowser.selectedTab = window.gBrowser.addTab(url);
    let linkedBrowser = tab.linkedBrowser;

    linkedBrowser.addEventListener("load", function onload() {
      linkedBrowser.removeEventListener("load", onload, true);
      resolve(tab);
    }, true);
  });

  return Promise.all([tabSwitchPromise, loadPromise]).then(([_, tab]) => tab);
}

/**
 * Helper function for charset tests. It loads |url| in a new tab,
 * runs |check1| in a ContentTask when the page is ready, switches the
 * charset to |charset|, and then runs |check2| in a ContentTask when
 * the page has finished reloading.
 *
 * |charset| and |check2| can be omitted, in which case the test
 * finishes when |check1| completes.
 */
function runCharsetTest(url, check1, charset, check2) {
  waitForExplicitFinish();

  BrowserTestUtils.openNewForegroundTab(gBrowser, url, true).then(afterOpen);

  function afterOpen() {
    if (charset) {
      BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(afterChangeCharset);

      ContentTask.spawn(gBrowser.selectedBrowser, null, check1).then(() => {
        BrowserSetForcedCharacterSet(charset);
      });
    } else {
      ContentTask.spawn(gBrowser.selectedBrowser, null, check1).then(() => {
        gBrowser.removeCurrentTab();
        finish();
      });
    }
  }

  function afterChangeCharset() {
    ContentTask.spawn(gBrowser.selectedBrowser, null, check2).then(() => {
      gBrowser.removeCurrentTab();
      finish();
    });
  }
}
