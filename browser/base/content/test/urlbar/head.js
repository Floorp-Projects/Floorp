/* eslint-env mozilla/frame-script */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm");
ChromeUtils.defineModuleGetter(this, "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm");
ChromeUtils.defineModuleGetter(this, "Preferences",
  "resource://gre/modules/Preferences.jsm");
ChromeUtils.defineModuleGetter(this, "HttpServer",
  "resource://testing-common/httpd.js");
ChromeUtils.defineModuleGetter(this, "SearchTestUtils",
  "resource://testing-common/SearchTestUtils.jsm");

SearchTestUtils.init(Assert, registerCleanupFunction);

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
  function content_script(contentStopFromProgressListener) {
    ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
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
      onStateChange(webProgress, req, flags, status) {
        dump("waitForDocLoadAndStopIt: onStateChange " + flags.toString(16) + ": " + req.name + "\n");

        if (webProgress.isTopLevel &&
            flags & Ci.nsIWebProgressListener.STATE_START) {
          wp.removeProgressListener(progressListener);

          let chan = req.QueryInterface(Ci.nsIChannel);
          dump(`waitForDocLoadAndStopIt: Document start: ${chan.URI.spec}\n`);

          stopContent(contentStopFromProgressListener, chan.originalURI.spec);
        }
      },
      QueryInterface: ChromeUtils.generateQI(["nsISupportsWeakReference"])
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

function is_element_visible(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(BrowserTestUtils.is_visible(element), msg || "Element should be visible");
}

function is_element_hidden(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(BrowserTestUtils.is_hidden(element), msg || "Element should be hidden");
}

function runHttpServer(scheme, host, port = -1) {
  let httpserver = new HttpServer();
  try {
    httpserver.start(port);
    port = httpserver.identity.primaryPort;
    httpserver.identity.setPrimary(scheme, host, port);
  } catch (ex) {
    info("We can't launch our http server successfully.");
  }
  is(httpserver.identity.has(scheme, host, port), true, `${scheme}://${host}:${port} is listening.`);
  return httpserver;
}

function promisePopupEvent(popup, eventSuffix) {
  let endState = {shown: "open", hidden: "closed"}[eventSuffix];

  if (popup.state == endState)
    return Promise.resolve();

  let eventType = "popup" + eventSuffix;
  return new Promise(resolve => {
    popup.addEventListener(eventType, function(event) {
      resolve();
    }, {once: true});

  });
}

function promisePopupShown(popup) {
  return promisePopupEvent(popup, "shown");
}

function promisePopupHidden(popup) {
  return promisePopupEvent(popup, "hidden");
}

function promiseSearchComplete(win = window, dontAnimate = false) {
  return promisePopupShown(win.gURLBar.popup).then(() => {
    function searchIsComplete() {
      let isComplete = win.gURLBar.controller.searchStatus >=
                       Ci.nsIAutoCompleteController.STATUS_COMPLETE_NO_MATCH;
      if (isComplete) {
        info(`Restore popup dontAnimate value to ${dontAnimate}`);
        win.gURLBar.popup.setAttribute("dontanimate", dontAnimate);
      }
      return isComplete;
    }

    // Wait until there are at least two matches.
    return BrowserTestUtils.waitForCondition(searchIsComplete, "waiting urlbar search to complete");
  });
}

function promiseAutocompleteResultPopup(inputText,
                                        win = window,
                                        fireInputEvent = false) {
  let dontAnimate = !!win.gURLBar.popup.getAttribute("dontanimate");
  waitForFocus(() => {
    info(`Disable popup animation. Change dontAnimate value from ${dontAnimate} to true.`);
    win.gURLBar.popup.setAttribute("dontanimate", "true");
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

  return promiseSearchComplete(win, dontAnimate);
}

function promisePageActionPanelOpen() {
  let dwu = window.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIDOMWindowUtils);
  return BrowserTestUtils.waitForCondition(() => {
    // Wait for the main page action button to become visible.  It's hidden for
    // some URIs, so depending on when this is called, it may not yet be quite
    // visible.  It's up to the caller to make sure it will be visible.
    info("Waiting for main page action button to have non-0 size");
    let bounds = dwu.getBoundsWithoutFlushing(BrowserPageActions.mainButtonNode);
    return bounds.width > 0 && bounds.height > 0;
  }).then(() => {
    // Wait for the panel to become open, by clicking the button if necessary.
    info("Waiting for main page action panel to be open");
    if (BrowserPageActions.panelNode.state == "open") {
      return Promise.resolve();
    }
    let shownPromise = promisePageActionPanelShown();
    EventUtils.synthesizeMouseAtCenter(BrowserPageActions.mainButtonNode, {});
    return shownPromise;
  }).then(() => {
    // Wait for items in the panel to become visible.
    return promisePageActionViewChildrenVisible(BrowserPageActions.mainViewNode);
  });
}

async function waitForActivatedActionPanel() {
  if (!BrowserPageActions.activatedActionPanelNode) {
    info("Waiting for activated-action panel to be added to mainPopupSet");
    await new Promise(resolve => {
      let observer = new MutationObserver(mutations => {
        if (BrowserPageActions.activatedActionPanelNode) {
          observer.disconnect();
          resolve();
        }
      });
      let popupSet = document.getElementById("mainPopupSet");
      observer.observe(popupSet, { childList: true });
    });
    info("Activated-action panel added to mainPopupSet");
  }
  if (!BrowserPageActions.activatedActionPanelNode.state == "open") {
    info("Waiting for activated-action panel popupshown");
    await promisePanelShown(BrowserPageActions.activatedActionPanelNode);
    info("Got activated-action panel popupshown");
  }
  let panelView =
    BrowserPageActions.activatedActionPanelNode.querySelector("panelview");
  if (panelView) {
    await BrowserTestUtils.waitForEvent(
      BrowserPageActions.activatedActionPanelNode,
      "ViewShown"
    );
    await promisePageActionViewChildrenVisible(panelView);
  }
  return panelView;
}

function promisePageActionPanelShown() {
  return promisePanelShown(BrowserPageActions.panelNode);
}

function promisePageActionPanelHidden() {
  return promisePanelHidden(BrowserPageActions.panelNode);
}

function promisePanelShown(panelIDOrNode) {
  return promisePanelEvent(panelIDOrNode, "popupshown");
}

function promisePanelHidden(panelIDOrNode) {
  return promisePanelEvent(panelIDOrNode, "popuphidden");
}

function promisePanelEvent(panelIDOrNode, eventType) {
  return new Promise(resolve => {
    let panel = panelIDOrNode;
    if (typeof panel == "string") {
      panel = document.getElementById(panelIDOrNode);
      if (!panel) {
        throw new Error(`Panel with ID "${panelIDOrNode}" does not exist.`);
      }
    }
    if ((eventType == "popupshown" && panel.state == "open") ||
        (eventType == "popuphidden" && panel.state == "closed")) {
      executeSoon(resolve);
      return;
    }
    panel.addEventListener(eventType, () => {
      executeSoon(resolve);
    }, { once: true });
  });
}

function promisePageActionViewShown() {
  info("promisePageActionViewShown waiting for ViewShown");
  return BrowserTestUtils.waitForEvent(BrowserPageActions.panelNode, "ViewShown").then(async event => {
    let panelViewNode = event.originalTarget;
    await promisePageActionViewChildrenVisible(panelViewNode);
    return panelViewNode;
  });
}

function promisePageActionViewChildrenVisible(panelViewNode) {
  return promiseNodeVisible(panelViewNode.firstChild.firstChild);
}

function promiseNodeVisible(node) {
  info(`promiseNodeVisible waiting, node.id=${node.id} node.localeName=${node.localName}\n`);
  let dwu = window.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIDOMWindowUtils);
  return BrowserTestUtils.waitForCondition(() => {
    let bounds = dwu.getBoundsWithoutFlushing(node);
    if (bounds.width > 0 && bounds.height > 0) {
      info(`promiseNodeVisible OK, node.id=${node.id} node.localeName=${node.localName}\n`);
      return true;
    }
    return false;
  });
}

function promiseSpeculativeConnection(httpserver) {
  return BrowserTestUtils.waitForCondition(() => {
    if (httpserver) {
      return httpserver.connectionNumber == 1;
    }
    return false;
  }, "Waiting for connection setup");
}

async function waitForAutocompleteResultAt(index) {
  let searchString = gURLBar.controller.searchString;
  await BrowserTestUtils.waitForCondition(
    () => gURLBar.popup.richlistbox.children.length > index &&
          gURLBar.popup.richlistbox.children[index].getAttribute("ac-text") == searchString,
    `Waiting for the autocomplete result for "${searchString}" at [${index}] to appear`);
  // Ensure the addition is complete, for proper mouse events on the entries.
  await new Promise(resolve => window.requestIdleCallback(resolve, {timeout: 1000}));
  return gURLBar.popup.richlistbox.children[index];
}

function promiseSuggestionsPresent(msg = "") {
  return TestUtils.waitForCondition(suggestionsPresent,
                                    msg || "Waiting for suggestions");
}

function suggestionsPresent() {
  let controller = gURLBar.popup.input.controller;
  let matchCount = controller.matchCount;
  for (let i = 0; i < matchCount; i++) {
    let url = controller.getValueAt(i);
    let mozActionMatch = url.match(/^moz-action:([^,]+),(.*)$/);
    if (mozActionMatch) {
      let [, type, paramStr] = mozActionMatch;
      let params = JSON.parse(paramStr);
      if (type == "searchengine" && "searchSuggestion" in params) {
        return true;
      }
    }
  }
  return false;
}
