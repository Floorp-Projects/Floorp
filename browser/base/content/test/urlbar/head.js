Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TabCrashHandler",
  "resource:///modules/ContentCrashHandlers.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
  "resource://gre/modules/Preferences.jsm");

function waitForCondition(condition, nextTest, errorMsg, retryTimes) {
  retryTimes = typeof retryTimes !== 'undefined' ?  retryTimes : 30;
  var tries = 0;
  var interval = setInterval(function() {
    if (tries >= retryTimes) {
      ok(false, errorMsg);
      moveOn();
    }
    var conditionPassed;
    try {
      conditionPassed = condition();
    } catch (e) {
      ok(false, e + "\n" + e.stack);
      conditionPassed = false;
    }
    if (conditionPassed) {
      moveOn();
    }
    tries++;
  }, 100);
  var moveOn = function() { clearInterval(interval); nextTest(); };
}

function promiseWaitForCondition(aConditionFn) {
  let deferred = Promise.defer();
  waitForCondition(aConditionFn, deferred.resolve, "Condition didn't pass.");
  return deferred.promise;
}

function promiseWaitForEvent(object, eventName, capturing = false, chrome = false) {
  return new Promise((resolve) => {
    function listener(event) {
      info("Saw " + eventName);
      object.removeEventListener(eventName, listener, capturing, chrome);
      resolve(event);
    }

    info("Waiting for " + eventName);
    object.addEventListener(eventName, listener, capturing, chrome);
  });
}

function promiseWindowWillBeClosed(win) {
  return new Promise((resolve, reject) => {
    Services.obs.addObserver(function observe(subject, topic) {
      if (subject == win) {
        Services.obs.removeObserver(observe, topic);
        resolve();
      }
    }, "domwindowclosed", false);
  });
}

function promiseWindowClosed(win) {
  let promise = promiseWindowWillBeClosed(win);
  win.close();
  return promise;
}

function promiseOpenAndLoadWindow(aOptions, aWaitForDelayedStartup=false) {
  let deferred = Promise.defer();
  let win = OpenBrowserWindow(aOptions);
  if (aWaitForDelayedStartup) {
    Services.obs.addObserver(function onDS(aSubject, aTopic, aData) {
      if (aSubject != win) {
        return;
      }
      Services.obs.removeObserver(onDS, "browser-delayed-startup-finished");
      deferred.resolve(win);
    }, "browser-delayed-startup-finished", false);

  } else {
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad);
      deferred.resolve(win);
    });
  }
  return deferred.promise;
}

function whenNewTabLoaded(aWindow, aCallback) {
  aWindow.BrowserOpenTab();

  let browser = aWindow.gBrowser.selectedBrowser;
  if (browser.contentDocument.readyState === "complete") {
    aCallback();
    return;
  }

  whenTabLoaded(aWindow.gBrowser.selectedTab, aCallback);
}

function whenTabLoaded(aTab, aCallback) {
  promiseTabLoadEvent(aTab).then(aCallback);
}

function promiseTabLoaded(aTab) {
  let deferred = Promise.defer();
  whenTabLoaded(aTab, deferred.resolve);
  return deferred.promise;
}

/**
 * Waits for the next top-level document load in the current browser.  The URI
 * of the document is compared against aExpectedURL.  The load is then stopped
 * before it actually starts.
 *
 * @param aExpectedURL
 *        The URL of the document that is expected to load.
 * @param aStopFromProgressListener
 *        Whether to cancel the load directly from the progress listener. Defaults to true.
 *        If you're using this method to avoid hitting the network, you want the default (true).
 *        However, the browser UI will behave differently for loads stopped directly from
 *        the progress listener (effectively in the middle of a call to loadURI) and so there
 *        are cases where you may want to avoid stopping the load directly from within the
 *        progress listener callback.
 * @return promise
 */
function waitForDocLoadAndStopIt(aExpectedURL, aBrowser=gBrowser.selectedBrowser, aStopFromProgressListener=true) {
  function content_script(aStopFromProgressListener) {
    let { interfaces: Ci, utils: Cu } = Components;
    Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
    let wp = docShell.QueryInterface(Ci.nsIWebProgress);

    function stopContent(now, uri) {
      if (now) {
        /* Hammer time. */
        content.stop();

        /* Let the parent know we're done. */
        sendAsyncMessage("Test:WaitForDocLoadAndStopIt", { uri });
      } else {
        setTimeout(stopContent.bind(null, true, uri), 0);
      }
    }

    let progressListener = {
      onStateChange: function (webProgress, req, flags, status) {
        dump("waitForDocLoadAndStopIt: onStateChange " + flags.toString(16) + ": " + req.name + "\n");

        if (webProgress.isTopLevel &&
            flags & Ci.nsIWebProgressListener.STATE_START) {
          wp.removeProgressListener(progressListener);

          let chan = req.QueryInterface(Ci.nsIChannel);
          dump(`waitForDocLoadAndStopIt: Document start: ${chan.URI.spec}\n`);

          stopContent(aStopFromProgressListener, chan.originalURI.spec);
        }
      },
      QueryInterface: XPCOMUtils.generateQI(["nsISupportsWeakReference"])
    };
    wp.addProgressListener(progressListener, wp.NOTIFY_STATE_WINDOW);

    /**
     * As |this| is undefined and we can't extend |docShell|, adding an unload
     * event handler is the easiest way to ensure the weakly referenced
     * progress listener is kept alive as long as necessary.
     */
    addEventListener("unload", function () {
      try {
        wp.removeProgressListener(progressListener);
      } catch (e) { /* Will most likely fail. */ }
    });
  }

  return new Promise((resolve, reject) => {
    function complete({ data }) {
      is(data.uri, aExpectedURL, "waitForDocLoadAndStopIt: The expected URL was loaded");
      mm.removeMessageListener("Test:WaitForDocLoadAndStopIt", complete);
      resolve();
    }

    let mm = aBrowser.messageManager;
    mm.loadFrameScript("data:,(" + content_script.toString() + ")(" + aStopFromProgressListener + ");", true);
    mm.addMessageListener("Test:WaitForDocLoadAndStopIt", complete);
    info("waitForDocLoadAndStopIt: Waiting for URL: " + aExpectedURL);
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
function promiseTabLoadEvent(tab, url)
{
  let deferred = Promise.defer();
  info("Wait tab event: load");

  function handle(loadedUrl) {
    if (loadedUrl === "about:blank" || (url && loadedUrl !== url)) {
      info(`Skipping spurious load event for ${loadedUrl}`);
      return false;
    }

    info("Tab event received: load");
    return true;
  }

  // Create two promises: one resolved from the content process when the page
  // loads and one that is rejected if we take too long to load the url.
  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, handle);

  let timeout = setTimeout(() => {
    deferred.reject(new Error("Timed out while waiting for a 'load' event"));
  }, 30000);

  loaded.then(() => {
    clearTimeout(timeout);
    deferred.resolve()
  });

  if (url)
    BrowserTestUtils.loadURI(tab.linkedBrowser, url);

  // Promise.all rejects if either promise rejects (i.e. if we time out) and
  // if our loaded promise resolves before the timeout, then we resolve the
  // timeout promise as well, causing the all promise to resolve.
  return Promise.all([deferred.promise, loaded]);
}

function makeActionURI(action, params) {
  let encodedParams = {};
  for (let key in params) {
    encodedParams[key] = encodeURIComponent(params[key]);
  }
  let url = "moz-action:" + action + "," + JSON.stringify(encodedParams);
  return NetUtil.newURI(url);
}

function is_hidden(element) {
  var style = element.ownerGlobal.getComputedStyle(element);
  if (style.display == "none")
    return true;
  if (style.visibility != "visible")
    return true;
  if (style.display == "-moz-popup")
    return ["hiding", "closed"].indexOf(element.state) != -1;

  // Hiding a parent element will hide all its children
  if (element.parentNode != element.ownerDocument)
    return is_hidden(element.parentNode);

  return false;
}

function is_visible(element) {
  var style = element.ownerGlobal.getComputedStyle(element);
  if (style.display == "none")
    return false;
  if (style.visibility != "visible")
    return false;
  if (style.display == "-moz-popup" && element.state != "open")
    return false;

  // Hiding a parent element will hide all its children
  if (element.parentNode != element.ownerDocument)
    return is_visible(element.parentNode);

  return true;
}

function is_element_visible(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(is_visible(element), msg || "Element should be visible");
}

function is_element_hidden(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(is_hidden(element), msg || "Element should be hidden");
}

function promisePopupEvent(popup, eventSuffix) {
  let endState = {shown: "open", hidden: "closed"}[eventSuffix];

  if (popup.state == endState)
    return Promise.resolve();

  let eventType = "popup" + eventSuffix;
  let deferred = Promise.defer();
  popup.addEventListener(eventType, function onPopupShown(event) {
    popup.removeEventListener(eventType, onPopupShown);
    deferred.resolve();
  });

  return deferred.promise;
}

function promisePopupShown(popup) {
  return promisePopupEvent(popup, "shown");
}

function promisePopupHidden(popup) {
  return promisePopupEvent(popup, "hidden");
}

function promiseSearchComplete(win = window) {
  return promisePopupShown(win.gURLBar.popup).then(() => {
    function searchIsComplete() {
      return win.gURLBar.controller.searchStatus >=
        Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH;
    }

    // Wait until there are at least two matches.
    return new Promise(resolve => waitForCondition(searchIsComplete, resolve));
  });
}

function promiseAutocompleteResultPopup(inputText,
                                        win = window,
                                        fireInputEvent = false) {
  waitForFocus(() => {
    win.gURLBar.focus();
    win.gURLBar.value = inputText;
    if (fireInputEvent) {
      // This is necessary to get the urlbar to set gBrowser.userTypedValue.
      let event = document.createEvent("Events");
      event.initEvent("input", true, true);
      win.gURLBar.dispatchEvent(event);
    }
    win.gURLBar.controller.startSearch(inputText);
  }, win);

  return promiseSearchComplete(win);
}

function promiseNewSearchEngine(basename) {
  return new Promise((resolve, reject) => {
    info("Waiting for engine to be added: " + basename);
    let url = getRootDirectory(gTestPath) + basename;
    Services.search.addEngine(url, null, "", false, {
      onSuccess: function (engine) {
        info("Search engine added: " + basename);
        registerCleanupFunction(() => Services.search.removeEngine(engine));
        resolve(engine);
      },
      onError: function (errCode) {
        Assert.ok(false, "addEngine failed with error code " + errCode);
        reject();
      },
    });
  });
}

