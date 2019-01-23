/* eslint-env mozilla/frame-script */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  HttpServer: "resource://testing-common/httpd.js",
  PlacesUtils: "resource://gre/modules/PlacesUtils.jsm",
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.jsm",
  Preferences: "resource://gre/modules/Preferences.jsm",
  SearchTestUtils: "resource://testing-common/SearchTestUtils.jsm",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
});

SearchTestUtils.init(Assert, registerCleanupFunction);

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

function promisePopupShown(popup) {
  return BrowserTestUtils.waitForPopupEvent(popup, "shown");
}

function promisePopupHidden(popup) {
  return BrowserTestUtils.waitForPopupEvent(popup, "hidden");
}

function promiseSearchComplete(win = window, dontAnimate = false) {
  return UrlbarTestUtils.promiseSearchComplete(win, dontAnimate);
}

function promiseAutocompleteResultPopup(inputText,
                                        win = window,
                                        fireInputEvent = false) {
  return UrlbarTestUtils.promiseAutocompleteResultPopup(inputText,
    win, waitForFocus, fireInputEvent);
}

function promisePageActionPanelOpen() {
  let dwu = window.windowUtils;
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
  return promiseNodeVisible(panelViewNode.firstElementChild.firstElementChild);
}

function promiseNodeVisible(node) {
  info(`promiseNodeVisible waiting, node.id=${node.id} node.localeName=${node.localName}\n`);
  let dwu = window.windowUtils;
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
  return UrlbarTestUtils.promiseSpeculativeConnection(httpserver);
}

async function waitForAutocompleteResultAt(index) {
  return UrlbarTestUtils.waitForAutocompleteResultAt(window, index);
}

function promiseSuggestionsPresent(msg = "") {
  return UrlbarTestUtils.promiseSuggestionsPresent(window, msg);
}

function suggestionsPresent() {
  return UrlbarTestUtils.suggestionsPresent(window);
}
