Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
  "resource://gre/modules/Preferences.jsm");

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
function waitForDocLoadAndStopIt(aExpectedURL, aBrowser = gBrowser.selectedBrowser, aStopFromProgressListener = true) {
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
      onStateChange: function(webProgress, req, flags, status) {
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
    addEventListener("unload", function() {
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
    return BrowserTestUtils.waitForCondition(searchIsComplete, "waiting urlbar search to complete");
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
      onSuccess: function(engine) {
        info("Search engine added: " + basename);
        registerCleanupFunction(() => Services.search.removeEngine(engine));
        resolve(engine);
      },
      onError: function(errCode) {
        Assert.ok(false, "addEngine failed with error code " + errCode);
        reject();
      },
    });
  });
}

