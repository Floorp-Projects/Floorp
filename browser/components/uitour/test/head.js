Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UITour",
                                  "resource:///modules/UITour.jsm");


const SINGLE_TRY_TIMEOUT = 100;
const NUMBER_OF_TRIES = 30;

function waitForConditionPromise(condition, timeoutMsg, tryCount=NUMBER_OF_TRIES) {
  let defer = Promise.defer();
  let tries = 0;
  function checkCondition() {
    if (tries >= tryCount) {
      defer.reject(timeoutMsg);
    }
    var conditionPassed;
    try {
      conditionPassed = condition();
    } catch (e) {
      return defer.reject(e);
    }
    if (conditionPassed) {
      return defer.resolve();
    }
    tries++;
    setTimeout(checkCondition, SINGLE_TRY_TIMEOUT);
  }
  setTimeout(checkCondition, SINGLE_TRY_TIMEOUT);
  return defer.promise;
}

function waitForCondition(condition, nextTest, errorMsg) {
  waitForConditionPromise(condition, errorMsg).then(nextTest, (reason) => {
    ok(false, reason + (reason.stack ? "\n" + e.stack : ""));
  });
}

/**
 * Wrapper to partially transition tests to Task.
 */
function taskify(fun) {
  return (done) => {
    // Output the inner function name otherwise no name will be output.
    info("\t" + fun.name);
    return Task.spawn(fun).then(done, (reason) => {
      ok(false, reason);
      done();
    });
  };
}

function is_hidden(element) {
  var style = element.ownerDocument.defaultView.getComputedStyle(element, "");
  if (style.display == "none")
    return true;
  if (style.visibility != "visible")
    return true;
  if (style.display == "-moz-popup")
    return ["hiding","closed"].indexOf(element.state) != -1;

  // Hiding a parent element will hide all its children
  if (element.parentNode != element.ownerDocument)
    return is_hidden(element.parentNode);

  return false;
}

function is_visible(element) {
  var style = element.ownerDocument.defaultView.getComputedStyle(element, "");
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
  ok(is_visible(element), msg);
}

function waitForElementToBeVisible(element, nextTest, msg) {
  waitForCondition(() => is_visible(element),
                   () => {
                     ok(true, msg);
                     nextTest();
                   },
                   "Timeout waiting for visibility: " + msg);
}

function waitForElementToBeHidden(element, nextTest, msg) {
  waitForCondition(() => is_hidden(element),
                   () => {
                     ok(true, msg);
                     nextTest();
                   },
                   "Timeout waiting for invisibility: " + msg);
}

function elementVisiblePromise(element, msg) {
  return waitForConditionPromise(() => is_visible(element), "Timeout waiting for visibility: " + msg);
}

function elementHiddenPromise(element, msg) {
  return waitForConditionPromise(() => is_hidden(element), "Timeout waiting for invisibility: " + msg);
}

function waitForPopupAtAnchor(popup, anchorNode, nextTest, msg) {
  waitForCondition(() => is_visible(popup) && popup.popupBoxObject.anchorNode == anchorNode,
                   () => {
                     ok(true, msg);
                     is_element_visible(popup, "Popup should be visible");
                     nextTest();
                   },
                   "Timeout waiting for popup at anchor: " + msg);
}

function hideInfoPromise(...args) {
  let popup = document.getElementById("UITourTooltip");
  gContentAPI.hideInfo.apply(gContentAPI, args);
  return promisePanelElementHidden(window, popup);
}

function showInfoPromise(...args) {
  let popup = document.getElementById("UITourTooltip");
  gContentAPI.showInfo.apply(gContentAPI, args);
  return promisePanelElementShown(window, popup);
}

function showHighlightPromise(...args) {
  let popup = document.getElementById("UITourHighlightContainer");
  gContentAPI.showHighlight.apply(gContentAPI, args);
  return promisePanelElementShown(window, popup);
}

function showMenuPromise(name) {
  return new Promise(resolve => {
    gContentAPI.showMenu(name, () => resolve());
  });
}

function waitForCallbackResultPromise() {
  return waitForConditionPromise(() => {
    return gContentWindow.callbackResult;
  }, "callback should be called");
}

function promisePanelShown(win) {
  let panelEl = win.PanelUI.panel;
  return promisePanelElementShown(win, panelEl);
}

function promisePanelElementEvent(win, aPanel, aEvent) {
  let deferred = Promise.defer();
  let timeoutId = win.setTimeout(() => {
    deferred.reject("Panel did not show within 5 seconds.");
  }, 5000);
  aPanel.addEventListener(aEvent, function onPanelEvent(e) {
    aPanel.removeEventListener(aEvent, onPanelEvent);
    win.clearTimeout(timeoutId);
    deferred.resolve();
  });
  return deferred.promise;
}

function promisePanelElementShown(win, aPanel) {
  return promisePanelElementEvent(win, aPanel, "popupshown");
}

function promisePanelElementHidden(win, aPanel) {
  return promisePanelElementEvent(win, aPanel, "popuphidden");
}

function is_element_hidden(element, msg) {
  isnot(element, null, "Element should not be null, when checking visibility");
  ok(is_hidden(element), msg);
}

function isTourBrowser(aBrowser) {
  let chromeWindow = aBrowser.ownerDocument.defaultView;
  return UITour.tourBrowsersByWindow.has(chromeWindow) &&
         UITour.tourBrowsersByWindow.get(chromeWindow).has(aBrowser);
}

function promisePageEvent() {
  return new Promise((resolve) => {
    Services.mm.addMessageListener("UITour:onPageEvent", function onPageEvent(aMessage) {
      Services.mm.removeMessageListener("UITour:onPageEvent", onPageEvent);
      SimpleTest.executeSoon(resolve);
    });
  });
}

function loadUITourTestPage(callback, host = "https://example.com/") {
  if (gTestTab)
    gBrowser.removeTab(gTestTab);

  let url = getRootDirectory(gTestPath) + "uitour.html";
  url = url.replace("chrome://mochitests/content/", host);

  gTestTab = gBrowser.addTab(url);
  gBrowser.selectedTab = gTestTab;

  gTestTab.linkedBrowser.addEventListener("load", function onLoad() {
    gTestTab.linkedBrowser.removeEventListener("load", onLoad, true);

    gContentWindow = Components.utils.waiveXrays(gTestTab.linkedBrowser.contentDocument.defaultView);
    gContentAPI = gContentWindow.Mozilla.UITour;

    waitForFocus(callback, gContentWindow);
  }, true);
}

function UITourTest() {
  Services.prefs.setBoolPref("browser.uitour.enabled", true);
  let testUri = Services.io.newURI("http://example.com", null, null);
  Services.perms.add(testUri, "uitour", Services.perms.ALLOW_ACTION);

  waitForExplicitFinish();

  registerCleanupFunction(function() {
    delete window.UITour;
    delete window.UITourMetricsProvider;
    delete window.gContentWindow;
    delete window.gContentAPI;
    if (gTestTab)
      gBrowser.removeTab(gTestTab);
    delete window.gTestTab;
    Services.prefs.clearUserPref("browser.uitour.enabled", true);
    Services.perms.remove("example.com", "uitour");
  });

  function done() {
    executeSoon(() => {
      if (gTestTab)
        gBrowser.removeTab(gTestTab);
      gTestTab = null;

      let highlight = document.getElementById("UITourHighlightContainer");
      is_element_hidden(highlight, "Highlight should be closed/hidden after UITour tab is closed");

      let tooltip = document.getElementById("UITourTooltip");
      is_element_hidden(tooltip, "Tooltip should be closed/hidden after UITour tab is closed");

      ok(!PanelUI.panel.hasAttribute("noautohide"), "@noautohide on the menu panel should have been cleaned up");
      ok(!PanelUI.panel.hasAttribute("panelopen"), "The panel shouldn't have @panelopen");
      isnot(PanelUI.panel.state, "open", "The panel shouldn't be open");
      is(document.getElementById("PanelUI-menu-button").hasAttribute("open"), false, "Menu button should know that the menu is closed");

      executeSoon(nextTest);
    });
  }

  function nextTest() {
    if (tests.length == 0) {
      finish();
      return;
    }
    let test = tests.shift();
    info("Starting " + test.name);
    waitForFocus(function() {
      loadUITourTestPage(function() {
        test(done);
      });
    });
  }
  nextTest();
}
