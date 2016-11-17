"use strict";

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "UITour",
                                  "resource:///modules/UITour.jsm");


const SINGLE_TRY_TIMEOUT = 100;
const NUMBER_OF_TRIES = 30;

function waitForConditionPromise(condition, timeoutMsg, tryCount = NUMBER_OF_TRIES) {
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
    return undefined;
  }
  setTimeout(checkCondition, SINGLE_TRY_TIMEOUT);
  return defer.promise;
}

function waitForCondition(condition, nextTestFn, errorMsg) {
  waitForConditionPromise(condition, errorMsg).then(nextTestFn, (reason) => {
    ok(false, reason + (reason.stack ? "\n" + reason.stack : ""));
  });
}

/**
 * Wrapper to partially transition tests to Task. Use `add_UITour_task` instead for new tests.
 */
function taskify(fun) {
  return (doneFn) => {
    // Output the inner function name otherwise no name will be output.
    info("\t" + fun.name);
    return Task.spawn(fun).then(doneFn, (reason) => {
      ok(false, reason);
      doneFn();
    });
  };
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
  ok(is_visible(element), msg);
}

function waitForElementToBeVisible(element, nextTestFn, msg) {
  waitForCondition(() => is_visible(element),
                   () => {
                     ok(true, msg);
                     nextTestFn();
                   },
                   "Timeout waiting for visibility: " + msg);
}

function waitForElementToBeHidden(element, nextTestFn, msg) {
  waitForCondition(() => is_hidden(element),
                   () => {
                     ok(true, msg);
                     nextTestFn();
                   },
                   "Timeout waiting for invisibility: " + msg);
}

function elementVisiblePromise(element, msg) {
  return waitForConditionPromise(() => is_visible(element), "Timeout waiting for visibility: " + msg);
}

function elementHiddenPromise(element, msg) {
  return waitForConditionPromise(() => is_hidden(element), "Timeout waiting for invisibility: " + msg);
}

function waitForPopupAtAnchor(popup, anchorNode, nextTestFn, msg) {
  waitForCondition(() => is_visible(popup) && popup.popupBoxObject.anchorNode == anchorNode,
                   () => {
                     ok(true, msg);
                     is_element_visible(popup, "Popup should be visible");
                     nextTestFn();
                   },
                   "Timeout waiting for popup at anchor: " + msg);
}

function getConfigurationPromise(configName) {
  return ContentTask.spawn(gTestTab.linkedBrowser, configName, contentConfigName => {
    return new Promise((resolve) => {
      let contentWin = Components.utils.waiveXrays(content);
      contentWin.Mozilla.UITour.getConfiguration(contentConfigName, resolve);
    });
  });
}

function hideInfoPromise(...args) {
  let popup = document.getElementById("UITourTooltip");
  gContentAPI.hideInfo.apply(gContentAPI, args);
  return promisePanelElementHidden(window, popup);
}

/**
 * `buttons` and `options` require functions from the content scope so we take a
 * function name to call to generate the buttons/options instead of the
 * buttons/options themselves. This makes the signature differ from the content one.
 */
function showInfoPromise(target, title, text, icon, buttonsFunctionName, optionsFunctionName) {
  let popup = document.getElementById("UITourTooltip");
  let shownPromise = promisePanelElementShown(window, popup);
  return ContentTask.spawn(gTestTab.linkedBrowser, [...arguments], args => {
    let contentWin = Components.utils.waiveXrays(content);
    let [contentTarget,
         contentTitle,
         contentText,
         contentIcon,
         contentButtonsFunctionName,
         contentOptionsFunctionName] = args;
    let buttons = contentButtonsFunctionName ? contentWin[contentButtonsFunctionName]() : null;
    let options = contentOptionsFunctionName ? contentWin[contentOptionsFunctionName]() : null;
    contentWin.Mozilla.UITour.showInfo(contentTarget, contentTitle, contentText, contentIcon, buttons, options);
  }).then(() => shownPromise);
}

function showHighlightPromise(...args) {
  let popup = document.getElementById("UITourHighlightContainer");
  gContentAPI.showHighlight.apply(gContentAPI, args);
  return promisePanelElementShown(window, popup);
}

function showMenuPromise(name) {
  return ContentTask.spawn(gTestTab.linkedBrowser, name, contentName => {
    return new Promise((resolve) => {
      let contentWin = Components.utils.waiveXrays(content);
      contentWin.Mozilla.UITour.showMenu(contentName, resolve);
    });
  });
}

function waitForCallbackResultPromise() {
  return ContentTask.spawn(gTestTab.linkedBrowser, null, function*() {
    let contentWin = Components.utils.waiveXrays(content);
    yield ContentTaskUtils.waitForCondition(() => {
      return contentWin.callbackResult;
    }, "callback should be called");
    return {
      data: contentWin.callbackData,
      result: contentWin.callbackResult,
    };
  });
}

function promisePanelShown(win) {
  let panelEl = win.PanelUI.panel;
  return promisePanelElementShown(win, panelEl);
}

function promisePanelElementEvent(win, aPanel, aEvent) {
  return new Promise((resolve, reject) => {
    let timeoutId = win.setTimeout(() => {
      aPanel.removeEventListener(aEvent, onPanelEvent);
      reject(aEvent + " event did not happen within 5 seconds.");
    }, 5000);

    function onPanelEvent(e) {
      aPanel.removeEventListener(aEvent, onPanelEvent);
      win.clearTimeout(timeoutId);
      // Wait one tick to let UITour.jsm process the event as well.
      executeSoon(resolve);
    }

    aPanel.addEventListener(aEvent, onPanelEvent);
  });
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
  let chromeWindow = aBrowser.ownerGlobal;
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

function loadUITourTestPage(callback, host = "https://example.org/") {
  if (gTestTab)
    gBrowser.removeTab(gTestTab);

  let url = getRootDirectory(gTestPath) + "uitour.html";
  url = url.replace("chrome://mochitests/content/", host);

  gTestTab = gBrowser.addTab(url);
  gBrowser.selectedTab = gTestTab;

  gTestTab.linkedBrowser.addEventListener("load", function onLoad() {
    gTestTab.linkedBrowser.removeEventListener("load", onLoad, true);

    if (gMultiProcessBrowser) {
      // When e10s is enabled, make gContentAPI and gContentWindow proxies which has every property
      // return a function which calls the method of the same name on
      // contentWin.Mozilla.UITour/contentWin in a ContentTask.
      let contentWinHandler = {
        get(target, prop, receiver) {
          return (...args) => {
            let taskArgs = {
              methodName: prop,
              args,
            };
            return ContentTask.spawn(gTestTab.linkedBrowser, taskArgs, contentArgs => {
              let contentWin = Components.utils.waiveXrays(content);
              return contentWin[contentArgs.methodName].apply(contentWin, contentArgs.args);
            });
          };
        },
      };
      gContentWindow = new Proxy({}, contentWinHandler);

      let UITourHandler = {
        get(target, prop, receiver) {
          return (...args) => {
            let browser = gTestTab.linkedBrowser;
            const proxyFunctionName = "UITourHandler:proxiedfunction-";
            // We need to proxy any callback functions using messages:
            let callbackMap = new Map();
            let fnIndices = [];
            args = args.map((arg, index) => {
              // Replace function arguments with "", and add them to the list of
              // forwarded functions. We'll construct a function on the content-side
              // that forwards all its arguments to a message, and we'll listen for
              // those messages on our side and call the corresponding function with
              // the arguments we got from the content side.
              if (typeof arg == "function") {
                callbackMap.set(index, arg);
                fnIndices.push(index);
                let handler = function(msg) {
                  // Please note that this handler assumes that the callback is used only once.
                  // That means that a single gContentAPI.observer() call can't be used to observe
                  // multiple events.
                  browser.messageManager.removeMessageListener(proxyFunctionName + index, handler);
                  callbackMap.get(index).apply(null, msg.data);
                };
                browser.messageManager.addMessageListener(proxyFunctionName + index, handler);
                return "";
              }
              return arg;
            });
            let taskArgs = {
              methodName: prop,
              args,
              fnIndices,
            };
            return ContentTask.spawn(browser, taskArgs, function*(contentArgs) {
              let contentWin = Components.utils.waiveXrays(content);
              let callbacksCalled = 0;
              let resolveCallbackPromise;
              let allCallbacksCalledPromise = new Promise(resolve => resolveCallbackPromise = resolve);
              let argumentsWithFunctions = contentArgs.args.map((arg, index) => {
                if (arg === "" && contentArgs.fnIndices.includes(index)) {
                  return function() {
                    callbacksCalled++;
                    sendAsyncMessage("UITourHandler:proxiedfunction-" + index, Array.from(arguments));
                    if (callbacksCalled >= contentArgs.fnIndices.length) {
                      resolveCallbackPromise();
                    }
                  };
                }
                return arg;
              });
              let rv = contentWin.Mozilla.UITour[contentArgs.methodName].apply(contentWin.Mozilla.UITour,
                                                                        argumentsWithFunctions);
              if (contentArgs.fnIndices.length) {
                yield allCallbacksCalledPromise;
              }
              return rv;
            });
          };
        },
      };
      gContentAPI = new Proxy({}, UITourHandler);
    } else {
      gContentWindow = Components.utils.waiveXrays(gTestTab.linkedBrowser.contentDocument.defaultView);
      gContentAPI = gContentWindow.Mozilla.UITour;
    }

    waitForFocus(callback, gTestTab.linkedBrowser);
  }, true);
}

// Wrapper for UITourTest to be used by add_task tests.
function* setup_UITourTest() {
  return UITourTest(true);
}

// Use `add_task(setup_UITourTest);` instead as we will fold this into `setup_UITourTest` once all tests are using `add_UITour_task`.
function UITourTest(usingAddTask = false) {
  Services.prefs.setBoolPref("browser.uitour.enabled", true);
  let testHttpsUri = Services.io.newURI("https://example.org", null, null);
  let testHttpUri = Services.io.newURI("http://example.org", null, null);
  Services.perms.add(testHttpsUri, "uitour", Services.perms.ALLOW_ACTION);
  Services.perms.add(testHttpUri, "uitour", Services.perms.ALLOW_ACTION);

  // If a test file is using add_task, we don't need to have a test function or
  // call `waitForExplicitFinish`.
  if (!usingAddTask) {
    waitForExplicitFinish();
  }

  registerCleanupFunction(function() {
    delete window.gContentWindow;
    delete window.gContentAPI;
    if (gTestTab)
      gBrowser.removeTab(gTestTab);
    delete window.gTestTab;
    Services.prefs.clearUserPref("browser.uitour.enabled");
    Services.perms.remove(testHttpsUri, "uitour");
    Services.perms.remove(testHttpUri, "uitour");
  });

  // When using tasks, the harness will call the next added task for us.
  if (!usingAddTask) {
    nextTest();
  }
}

function done(usingAddTask = false) {
  info("== Done test, doing shared checks before teardown ==");
  return new Promise((resolve) => {
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

      info("Done shared checks");
      if (usingAddTask) {
        executeSoon(resolve);
      } else {
        executeSoon(nextTest);
      }
    });
  });
}

function nextTest() {
  if (tests.length == 0) {
    info("finished tests in this file");
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

/**
 * All new tests that need the help of `loadUITourTestPage` should use this
 * wrapper around their test's generator function to reduce boilerplate.
 */
function add_UITour_task(func) {
  let genFun = function*() {
    yield new Promise((resolve) => {
      waitForFocus(function() {
        loadUITourTestPage(function() {
          let funcPromise = Task.spawn(func)
                                .then(() => done(true),
                                      (reason) => {
                                        ok(false, reason);
                                        return done(true);
                                      });
          resolve(funcPromise);
        });
      });
    });
  };
  Object.defineProperty(genFun, "name", {
    configurable: true,
    value: func.name,
  });
  add_task(genFun);
}
