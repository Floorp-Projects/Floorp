/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let tempScope = {};
Cu.import("resource:///modules/HUDService.jsm", tempScope);
let HUDService = tempScope.HUDService;
Cu.import("resource://gre/modules/devtools/WebConsoleUtils.jsm", tempScope);
let WebConsoleUtils = tempScope.WebConsoleUtils;
Cu.import("resource:///modules/devtools/gDevTools.jsm", tempScope);
let gDevTools = tempScope.gDevTools;
Cu.import("resource:///modules/devtools/Target.jsm", tempScope);
let TargetFactory = tempScope.TargetFactory;
Components.utils.import("resource://gre/modules/devtools/Console.jsm", tempScope);
let console = tempScope.console;

const WEBCONSOLE_STRINGS_URI = "chrome://browser/locale/devtools/webconsole.properties";
let WCU_l10n = new WebConsoleUtils.l10n(WEBCONSOLE_STRINGS_URI);

function log(aMsg)
{
  dump("*** WebConsoleTest: " + aMsg + "\n");
}

function pprint(aObj)
{
  for (let prop in aObj) {
    if (typeof aObj[prop] == "function") {
      log("function " + prop);
    }
    else {
      log(prop + ": " + aObj[prop]);
    }
  }
}

let tab, browser, hudId, hud, hudBox, filterBox, outputNode, cs;

function addTab(aURL)
{
  gBrowser.selectedTab = gBrowser.addTab(aURL);
  tab = gBrowser.selectedTab;
  browser = gBrowser.getBrowserForTab(tab);
}

function afterAllTabsLoaded(callback, win) {
  win = win || window;

  let stillToLoad = 0;

  function onLoad() {
    this.removeEventListener("load", onLoad, true);
    stillToLoad--;
    if (!stillToLoad)
      callback();
  }

  for (let a = 0; a < win.gBrowser.tabs.length; a++) {
    let browser = win.gBrowser.tabs[a].linkedBrowser;
    if (browser.contentDocument.readyState != "complete") {
      stillToLoad++;
      browser.addEventListener("load", onLoad, true);
    }
  }

  if (!stillToLoad)
    callback();
}

/**
 * Check if a log entry exists in the HUD output node.
 *
 * @param {Element} aOutputNode
 *        the HUD output node.
 * @param {string} aMatchString
 *        the string you want to check if it exists in the output node.
 * @param {string} aMsg
 *        the message describing the test
 * @param {boolean} [aOnlyVisible=false]
 *        find only messages that are visible, not hidden by the filter.
 * @param {boolean} [aFailIfFound=false]
 *        fail the test if the string is found in the output node.
 * @param {string} aClass [optional]
 *        find only messages with the given CSS class.
 */
function testLogEntry(aOutputNode, aMatchString, aMsg, aOnlyVisible,
                      aFailIfFound, aClass)
{
  let selector = ".hud-msg-node";
  // Skip entries that are hidden by the filter.
  if (aOnlyVisible) {
    selector += ":not(.hud-filtered-by-type)";
  }
  if (aClass) {
    selector += "." + aClass;
  }

  let msgs = aOutputNode.querySelectorAll(selector);
  let found = false;
  for (let i = 0, n = msgs.length; i < n; i++) {
    let message = msgs[i].textContent.indexOf(aMatchString);
    if (message > -1) {
      found = true;
      break;
    }

    // Search the labels too.
    let labels = msgs[i].querySelectorAll("label");
    for (let j = 0; j < labels.length; j++) {
      if (labels[j].getAttribute("value").indexOf(aMatchString) > -1) {
        found = true;
        break;
      }
    }
  }

  is(found, !aFailIfFound, aMsg);
}

/**
 * A convenience method to call testLogEntry().
 *
 * @param string aString
 *        The string to find.
 */
function findLogEntry(aString)
{
  testLogEntry(outputNode, aString, "found " + aString);
}

/**
 * Open the Web Console for the given tab.
 *
 * @param nsIDOMElement [aTab]
 *        Optional tab element for which you want open the Web Console. The
 *        default tab is taken from the global variable |tab|.
 * @param function [aCallback]
 *        Optional function to invoke after the Web Console completes
 *        initialization (web-console-created).
 */
function openConsole(aTab, aCallback = function() { })
{
  let target = TargetFactory.forTab(aTab || tab);
  gDevTools.showToolbox(target, "webconsole").then(function(toolbox) {
    aCallback(toolbox.getCurrentPanel().hud);
  });
}

/**
 * Close the Web Console for the given tab.
 *
 * @param nsIDOMElement [aTab]
 *        Optional tab element for which you want close the Web Console. The
 *        default tab is taken from the global variable |tab|.
 * @param function [aCallback]
 *        Optional function to invoke after the Web Console completes
 *        closing (web-console-destroyed).
 */
function closeConsole(aTab, aCallback = function() { })
{
  let target = TargetFactory.forTab(aTab || tab);
  let toolbox = gDevTools.getToolbox(target);
  if (toolbox) {
    let panel = toolbox.getPanel("webconsole");
    if (panel) {
      let hudId = panel.hud.hudId;
      toolbox.destroy().then(function() {
        executeSoon(aCallback.bind(null, hudId));
      }).then(null, console.error);
    }
    else {
      toolbox.destroy().then(aCallback.bind(null));
    }
  }
  else {
    aCallback();
  }
}

/**
 * Polls a given function waiting for opening context menu.
 *
 * @Param {nsIDOMElement} aContextMenu
 * @param object aOptions
 *        Options object with the following properties:
 *        - successFn
 *        A function called if opening the given context menu - success to return.
 *        - failureFn
 *        A function called if not opening the given context menu - fails to return.
 *        - target
 *        The target element for showing a context menu.
 *        - timeout
 *        Timeout for popup shown, in milliseconds. Default is 5000.
 */
function waitForOpenContextMenu(aContextMenu, aOptions) {
  let start = Date.now();
  let timeout = aOptions.timeout || 5000;
  let targetElement = aOptions.target;

  if (!aContextMenu) {
    ok(false, "Can't get a context menu.");
    aOptions.failureFn();
    return;
  }
  if (!targetElement) {
    ok(false, "Can't get a target element.");
    aOptions.failureFn();
    return;
  }

  function onPopupShown() {
    aContextMenu.removeEventListener("popupshown", onPopupShown);
    clearTimeout(onTimeout);
    aOptions.successFn();
  }


  aContextMenu.addEventListener("popupshown", onPopupShown);

  let onTimeout = setTimeout(function(){
    aContextMenu.removeEventListener("popupshown", onPopupShown);
    aOptions.failureFn();
  }, timeout);

  // open a context menu.
  let eventDetails = { type : "contextmenu", button : 2};
  EventUtils.synthesizeMouse(targetElement, 2, 2,
                             eventDetails, targetElement.ownerDocument.defaultView);
}

function finishTest()
{
  browser = hudId = hud = filterBox = outputNode = cs = null;

  let hud = HUDService.getHudByWindow(content);
  if (!hud) {
    finish();
    return;
  }
  hud.jsterm.clearOutput(true);

  closeConsole(hud.tab, finish);

  hud = null;
}

function tearDown()
{
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  gDevTools.closeToolbox(target);
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
  WCU_l10n = tab = browser = hudId = hud = filterBox = outputNode = cs = null;
}

registerCleanupFunction(tearDown);

waitForExplicitFinish();

/**
 * Polls a given function waiting for it to become true.
 *
 * @param object aOptions
 *        Options object with the following properties:
 *        - validatorFn
 *        A validator function that returns a boolean. This is called every few
 *        milliseconds to check if the result is true. When it is true, succesFn
 *        is called and polling stops. If validatorFn never returns true, then
 *        polling timeouts after several tries and a failure is recorded.
 *        - successFn
 *        A function called when the validator function returns true.
 *        - failureFn
 *        A function called if the validator function timeouts - fails to return
 *        true in the given time.
 *        - name
 *        Name of test. This is used to generate the success and failure
 *        messages.
 *        - timeout
 *        Timeout for validator function, in milliseconds. Default is 5000.
 */
function waitForSuccess(aOptions)
{
  let start = Date.now();
  let timeout = aOptions.timeout || 5000;

  function wait(validatorFn, successFn, failureFn)
  {
    if ((Date.now() - start) > timeout) {
      // Log the failure.
      ok(false, "Timed out while waiting for: " + aOptions.name);
      failureFn(aOptions);
      return;
    }

    if (validatorFn(aOptions)) {
      ok(true, aOptions.name);
      successFn();
    }
    else {
      setTimeout(function() wait(validatorFn, successFn, failureFn), 100);
    }
  }

  wait(aOptions.validatorFn, aOptions.successFn, aOptions.failureFn);
}

function openInspector(aCallback, aTab = gBrowser.selectedTab)
{
  let target = TargetFactory.forTab(aTab);
  gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
    aCallback(toolbox.getCurrentPanel());
  });
}
