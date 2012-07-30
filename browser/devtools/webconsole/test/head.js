/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let tempScope = {};
Cu.import("resource:///modules/HUDService.jsm", tempScope);
let HUDService = tempScope.HUDService;
Cu.import("resource:///modules/WebConsoleUtils.jsm", tempScope);
let WebConsoleUtils = tempScope.WebConsoleUtils;

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
  gBrowser.selectedTab = gBrowser.addTab();
  content.location = aURL;
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
function openConsole(aTab, aCallback)
{
  function onWebConsoleOpen(aSubject, aTopic)
  {
    if (aTopic == "web-console-created") {
      Services.obs.removeObserver(onWebConsoleOpen, "web-console-created");
      aSubject.QueryInterface(Ci.nsISupportsString);
      let hud = HUDService.getHudReferenceById(aSubject.data);
      executeSoon(aCallback.bind(null, hud));
    }
  }

  if (aCallback) {
    Services.obs.addObserver(onWebConsoleOpen, "web-console-created", false);
  }

  HUDService.activateHUDForContext(aTab || tab);
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
function closeConsole(aTab, aCallback)
{
  function onWebConsoleClose(aSubject, aTopic)
  {
    if (aTopic == "web-console-destroyed") {
      Services.obs.removeObserver(onWebConsoleClose, "web-console-destroyed");
      aSubject.QueryInterface(Ci.nsISupportsString);
      let hudId = aSubject.data;
      executeSoon(aCallback.bind(null, hudId));
    }
  }

  if (aCallback) {
    Services.obs.addObserver(onWebConsoleClose, "web-console-destroyed", false);
  }

  HUDService.deactivateHUDForContext(aTab || tab);
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
  HUDService.deactivateHUDForContext(gBrowser.selectedTab);
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
  tab = browser = hudId = hud = filterBox = outputNode = cs = null;
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
