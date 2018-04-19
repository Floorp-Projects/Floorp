var {UrlClassifierTestUtils} = ChromeUtils.import("resource://testing-common/UrlClassifierTestUtils.jsm", {});

const PREF = "privacy.trackingprotection.enabled";
const PB_PREF = "privacy.trackingprotection.pbmode.enabled";
const ROOT = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://tracking.example.org");
const BENIGN_PAGE = ROOT + "benignPage.html";
const TRACKING_PAGE = ROOT + "trackingPage.html";

function promiseWindowWillBeClosed(win) {
  return new Promise((resolve, reject) => {
    Services.obs.addObserver(function observe(subject, topic) {
      if (subject == win) {
        Services.obs.removeObserver(observe, topic);
        executeSoon(resolve);
      }
    }, "domwindowclosed");
  });
}

function promiseWindowClosed(win) {
  let promise = promiseWindowWillBeClosed(win);
  win.close();
  return promise;
}

function promiseOpenAndLoadWindow(aOptions, aWaitForDelayedStartup = false) {
  return new Promise(resolve => {
    let win = OpenBrowserWindow(aOptions);
    if (aWaitForDelayedStartup) {
      Services.obs.addObserver(function onDS(aSubject, aTopic, aData) {
        if (aSubject != win) {
          return;
        }
        Services.obs.removeObserver(onDS, "browser-delayed-startup-finished");
        resolve(win);
      }, "browser-delayed-startup-finished");

    } else {
      win.addEventListener("load", function() {
        resolve(win);
      }, {once: true});
    }
  });
}

/**
 * Waits for a load (or custom) event to finish in a given tab. If provided
 * load an uri into the tab.
 *
 * @param tab
 *        The tab to load into.
 * @param [optional] url
 *        The url to load, or the current url.
 * @return {Promise} resolved when the event is handled.
 * @resolves to the received event
 * @rejects if a valid load event is not received within a meaningful interval
 */
function promiseTabLoadEvent(tab, url) {
  info("Wait tab event: load");

  function handle(loadedUrl) {
    if (loadedUrl === "about:blank" || (url && loadedUrl !== url)) {
      info(`Skipping spurious load event for ${loadedUrl}`);
      return false;
    }

    info("Tab event received: load");
    return true;
  }

  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, handle);

  if (url)
    BrowserTestUtils.loadURI(tab.linkedBrowser, url);

  return loaded;
}
