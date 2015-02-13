/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let {gDevTools} = Cu.import("resource:///modules/devtools/gDevTools.jsm", {});
let {console} = Cu.import("resource://gre/modules/devtools/Console.jsm", {});
let {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});
let {Task} = Cu.import("resource://gre/modules/Task.jsm", {});
let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let {require, TargetFactory} = devtools;
let {Utils: WebConsoleUtils} = require("devtools/toolkit/webconsole/utils");
let {Messages} = require("devtools/webconsole/console-output");

// promise._reportErrors = true; // please never leave me.
//Services.prefs.setBoolPref("devtools.debugger.log", true);

let gPendingOutputTest = 0;

// The various categories of messages.
const CATEGORY_NETWORK = 0;
const CATEGORY_CSS = 1;
const CATEGORY_JS = 2;
const CATEGORY_WEBDEV = 3;
const CATEGORY_INPUT = 4;
const CATEGORY_OUTPUT = 5;
const CATEGORY_SECURITY = 6;

// The possible message severities.
const SEVERITY_ERROR = 0;
const SEVERITY_WARNING = 1;
const SEVERITY_INFO = 2;
const SEVERITY_LOG = 3;

// The indent of a console group in pixels.
const GROUP_INDENT = 12;

const WEBCONSOLE_STRINGS_URI = "chrome://browser/locale/devtools/webconsole.properties";
let WCU_l10n = new WebConsoleUtils.l10n(WEBCONSOLE_STRINGS_URI);

gDevTools.testing = true;

function asyncTest(generator) {
  return () => {
    Task.spawn(generator).then(finishTest);
  };
}


function loadTab(url) {
  let deferred = promise.defer();

  let tab = gBrowser.selectedTab = gBrowser.addTab(url);
  let browser = gBrowser.getBrowserForTab(tab);

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    deferred.resolve({tab: tab, browser: browser});
  }, true);

  return deferred.promise;
}

function loadBrowser(browser) {
  let deferred = promise.defer();

  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    deferred.resolve(null);
  }, true);

  return deferred.promise;
}

function closeTab(tab) {
  let deferred = promise.defer();

  let container = gBrowser.tabContainer;

  container.addEventListener("TabClose", function onTabClose() {
    container.removeEventListener("TabClose", onTabClose, true);
    deferred.resolve(null);
  }, true);

  gBrowser.removeTab(tab);

  return deferred.promise;
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
    if (browser.webProgress.isLoadingDocument) {
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
  let selector = ".message";
  // Skip entries that are hidden by the filter.
  if (aOnlyVisible) {
    selector += ":not(.filtered-by-type):not(.filtered-by-string)";
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
 * @return object
 *         A promise that is resolved once the web console is open.
 */
let openConsole = function(aTab) {
  let webconsoleOpened = promise.defer();
  let target = TargetFactory.forTab(aTab || gBrowser.selectedTab);
  gDevTools.showToolbox(target, "webconsole").then(toolbox => {
    let hud = toolbox.getCurrentPanel().hud;
    hud.jsterm._lazyVariablesView = false;
    webconsoleOpened.resolve(hud);
  });
  return webconsoleOpened.promise;
};

/**
 * Close the Web Console for the given tab.
 *
 * @param nsIDOMElement [aTab]
 *        Optional tab element for which you want close the Web Console. The
 *        default tab is taken from the global variable |tab|.
 * @param function [aCallback]
 *        Optional function to invoke after the Web Console completes
 *        closing (web-console-destroyed).
 * @return object
 *         A promise that is resolved once the web console is closed.
 */
let closeConsole = Task.async(function* (aTab) {
  let target = TargetFactory.forTab(aTab || gBrowser.selectedTab);
  let toolbox = gDevTools.getToolbox(target);
  if (toolbox) {
    yield toolbox.destroy();
  }
});

/**
 * Wait for a context menu popup to open.
 *
 * @param nsIDOMElement aPopup
 *        The XUL popup you expect to open.
 * @param nsIDOMElement aButton
 *        The button/element that receives the contextmenu event. This is
 *        expected to open the popup.
 * @param function aOnShown
 *        Function to invoke on popupshown event.
 * @param function aOnHidden
 *        Function to invoke on popuphidden event.
 * @return object
 *         A Promise object that is resolved after the popuphidden event
 *         callback is invoked.
 */
function waitForContextMenu(aPopup, aButton, aOnShown, aOnHidden)
{
  function onPopupShown() {
    info("onPopupShown");
    aPopup.removeEventListener("popupshown", onPopupShown);

    aOnShown && aOnShown();

    // Use executeSoon() to get out of the popupshown event.
    aPopup.addEventListener("popuphidden", onPopupHidden);
    executeSoon(() => aPopup.hidePopup());
  }
  function onPopupHidden() {
    info("onPopupHidden");
    aPopup.removeEventListener("popuphidden", onPopupHidden);

    aOnHidden && aOnHidden();

    deferred.resolve(aPopup);
  }

  let deferred = promise.defer();
  aPopup.addEventListener("popupshown", onPopupShown);

  info("wait for the context menu to open");
  let eventDetails = { type: "contextmenu", button: 2};
  EventUtils.synthesizeMouse(aButton, 2, 2, eventDetails,
                             aButton.ownerDocument.defaultView);
  return deferred.promise;
}

/**
 * Dump the output of all open Web Consoles - used only for debugging purposes.
 */
function dumpConsoles()
{
  if (gPendingOutputTest) {
    console.log("dumpConsoles start");
    for (let [, hud] of HUDService.consoles) {
      if (!hud.outputNode) {
        console.debug("no output content for", hud.hudId);
        continue;
      }

      console.debug("output content for", hud.hudId);
      for (let elem of hud.outputNode.childNodes) {
        dumpMessageElement(elem);
      }
    }
    console.log("dumpConsoles end");

    gPendingOutputTest = 0;
  }
}

/**
 * Dump to output debug information for the given webconsole message.
 *
 * @param nsIDOMNode aMessage
 *        The message element you want to display.
 */
function dumpMessageElement(aMessage)
{
  let text = aMessage.textContent;
  let repeats = aMessage.querySelector(".message-repeats");
  if (repeats) {
    repeats = repeats.getAttribute("value");
  }
  console.debug("id", aMessage.getAttribute("id"),
                "date", aMessage.timestamp,
                "class", aMessage.className,
                "category", aMessage.category,
                "severity", aMessage.severity,
                "repeats", repeats,
                "clipboardText", aMessage.clipboardText,
                "text", text);
}

let finishTest = Task.async(function* () {
  dumpConsoles();

  let browserConsole = HUDService.getBrowserConsole();
  if (browserConsole) {
    if (browserConsole.jsterm) {
      browserConsole.jsterm.clearOutput(true);
    }
    yield HUDService.toggleBrowserConsole();
  }

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  yield gDevTools.closeToolbox(target);

  finish();
});

registerCleanupFunction(function*() {
  gDevTools.testing = false;

  dumpConsoles();

  if (HUDService.getBrowserConsole()) {
    HUDService.toggleBrowserConsole();
  }

  let target = TargetFactory.forTab(gBrowser.selectedTab);
  yield gDevTools.closeToolbox(target);

  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

waitForExplicitFinish();

/**
 * Polls a given function waiting for it to become true.
 *
 * @param object aOptions
 *        Options object with the following properties:
 *        - validator
 *        A validator function that returns a boolean. This is called every few
 *        milliseconds to check if the result is true. When it is true, the
 *        promise is resolved and polling stops. If validator never returns
 *        true, then polling timeouts after several tries and the promise is
 *        rejected.
 *        - name
 *        Name of test. This is used to generate the success and failure
 *        messages.
 *        - timeout
 *        Timeout for validator function, in milliseconds. Default is 5000.
 * @return object
 *         A Promise object that is resolved based on the validator function.
 */
function waitForSuccess(aOptions)
{
  let deferred = promise.defer();
  let start = Date.now();
  let timeout = aOptions.timeout || 5000;
  let {validator} = aOptions;


  function wait()
  {
    if ((Date.now() - start) > timeout) {
      // Log the failure.
      ok(false, "Timed out while waiting for: " + aOptions.name);
      deferred.reject(null);
      return;
    }

    if (validator(aOptions)) {
      ok(true, aOptions.name);
      deferred.resolve(null);
    }
    else {
      setTimeout(wait, 100);
    }
  }

  setTimeout(wait, 100);

  return deferred.promise;
}

let openInspector = Task.async(function* (aTab = gBrowser.selectedTab) {
  let target = TargetFactory.forTab(aTab);
  let toolbox = yield gDevTools.showToolbox(target, "inspector");
  return toolbox.getCurrentPanel();
});

/**
 * Find variables or properties in a VariablesView instance.
 *
 * @param object aView
 *        The VariablesView instance.
 * @param array aRules
 *        The array of rules you want to match. Each rule is an object with:
 *        - name (string|regexp): property name to match.
 *        - value (string|regexp): property value to match.
 *        - isIterator (boolean): check if the property is an iterator.
 *        - isGetter (boolean): check if the property is a getter.
 *        - isGenerator (boolean): check if the property is a generator.
 *        - dontMatch (boolean): make sure the rule doesn't match any property.
 * @param object aOptions
 *        Options for matching:
 *        - webconsole: the WebConsole instance we work with.
 * @return object
 *         A promise object that is resolved when all the rules complete
 *         matching. The resolved callback is given an array of all the rules
 *         you wanted to check. Each rule has a new property: |matchedProp|
 *         which holds a reference to the Property object instance from the
 *         VariablesView. If the rule did not match, then |matchedProp| is
 *         undefined.
 */
function findVariableViewProperties(aView, aRules, aOptions)
{
  // Initialize the search.
  function init()
  {
    // Separate out the rules that require expanding properties throughout the
    // view.
    let expandRules = [];
    let rules = aRules.filter((aRule) => {
      if (typeof aRule.name == "string" && aRule.name.indexOf(".") > -1) {
        expandRules.push(aRule);
        return false;
      }
      return true;
    });

    // Search through the view those rules that do not require any properties to
    // be expanded. Build the array of matchers, outstanding promises to be
    // resolved.
    let outstanding = [];
    finder(rules, aView, outstanding);

    // Process the rules that need to expand properties.
    let lastStep = processExpandRules.bind(null, expandRules);

    // Return the results - a promise resolved to hold the updated aRules array.
    let returnResults = onAllRulesMatched.bind(null, aRules);

    return promise.all(outstanding).then(lastStep).then(returnResults);
  }

  function onMatch(aProp, aRule, aMatched)
  {
    if (aMatched && !aRule.matchedProp) {
      aRule.matchedProp = aProp;
    }
  }

  function finder(aRules, aVar, aPromises)
  {
    for (let [id, prop] of aVar) {
      for (let rule of aRules) {
        let matcher = matchVariablesViewProperty(prop, rule, aOptions);
        aPromises.push(matcher.then(onMatch.bind(null, prop, rule)));
      }
    }
  }

  function processExpandRules(aRules)
  {
    let rule = aRules.shift();
    if (!rule) {
      return promise.resolve(null);
    }

    let deferred = promise.defer();
    let expandOptions = {
      rootVariable: aView,
      expandTo: rule.name,
      webconsole: aOptions.webconsole,
    };

    variablesViewExpandTo(expandOptions).then(function onSuccess(aProp) {
      let name = rule.name;
      let lastName = name.split(".").pop();
      rule.name = lastName;

      let matched = matchVariablesViewProperty(aProp, rule, aOptions);
      return matched.then(onMatch.bind(null, aProp, rule)).then(function() {
        rule.name = name;
      });
    }, function onFailure() {
      return promise.resolve(null);
    }).then(processExpandRules.bind(null, aRules)).then(function() {
      deferred.resolve(null);
    });

    return deferred.promise;
  }

  function onAllRulesMatched(aRules)
  {
    for (let rule of aRules) {
      let matched = rule.matchedProp;
      if (matched && !rule.dontMatch) {
        ok(true, "rule " + rule.name + " matched for property " + matched.name);
      }
      else if (matched && rule.dontMatch) {
        ok(false, "rule " + rule.name + " should not match property " +
           matched.name);
      }
      else {
        ok(rule.dontMatch, "rule " + rule.name + " did not match any property");
      }
    }
    return aRules;
  }

  return init();
}

/**
 * Check if a given Property object from the variables view matches the given
 * rule.
 *
 * @param object aProp
 *        The variable's view Property instance.
 * @param object aRule
 *        Rules for matching the property. See findVariableViewProperties() for
 *        details.
 * @param object aOptions
 *        Options for matching. See findVariableViewProperties().
 * @return object
 *         A promise that is resolved when all the checks complete. Resolution
 *         result is a boolean that tells your promise callback the match
 *         result: true or false.
 */
function matchVariablesViewProperty(aProp, aRule, aOptions)
{
  function resolve(aResult) {
    return promise.resolve(aResult);
  }

  if (aRule.name) {
    let match = aRule.name instanceof RegExp ?
                aRule.name.test(aProp.name) :
                aProp.name == aRule.name;
    if (!match) {
      return resolve(false);
    }
  }

  if (aRule.value) {
    let displayValue = aProp.displayValue;
    if (aProp.displayValueClassName == "token-string") {
      displayValue = displayValue.substring(1, displayValue.length - 1);
    }

    let match = aRule.value instanceof RegExp ?
                aRule.value.test(displayValue) :
                displayValue == aRule.value;
    if (!match) {
      info("rule " + aRule.name + " did not match value, expected '" +
           aRule.value + "', found '" + displayValue  + "'");
      return resolve(false);
    }
  }

  if ("isGetter" in aRule) {
    let isGetter = !!(aProp.getter && aProp.get("get"));
    if (aRule.isGetter != isGetter) {
      info("rule " + aRule.name + " getter test failed");
      return resolve(false);
    }
  }

  if ("isGenerator" in aRule) {
    let isGenerator = aProp.displayValue == "Generator";
    if (aRule.isGenerator != isGenerator) {
      info("rule " + aRule.name + " generator test failed");
      return resolve(false);
    }
  }

  let outstanding = [];

  if ("isIterator" in aRule) {
    let isIterator = isVariableViewPropertyIterator(aProp, aOptions.webconsole);
    outstanding.push(isIterator.then((aResult) => {
      if (aResult != aRule.isIterator) {
        info("rule " + aRule.name + " iterator test failed");
      }
      return aResult == aRule.isIterator;
    }));
  }

  outstanding.push(promise.resolve(true));

  return promise.all(outstanding).then(function _onMatchDone(aResults) {
    let ruleMatched = aResults.indexOf(false) == -1;
    return resolve(ruleMatched);
  });
}

/**
 * Check if the given variables view property is an iterator.
 *
 * @param object aProp
 *        The Property instance you want to check.
 * @param object aWebConsole
 *        The WebConsole instance to work with.
 * @return object
 *         A promise that is resolved when the check completes. The resolved
 *         callback is given a boolean: true if the property is an iterator, or
 *         false otherwise.
 */
function isVariableViewPropertyIterator(aProp, aWebConsole)
{
  if (aProp.displayValue == "Iterator") {
    return promise.resolve(true);
  }

  let deferred = promise.defer();

  variablesViewExpandTo({
    rootVariable: aProp,
    expandTo: "__proto__.__iterator__",
    webconsole: aWebConsole,
  }).then(function onSuccess(aProp) {
    deferred.resolve(true);
  }, function onFailure() {
    deferred.resolve(false);
  });

  return deferred.promise;
}


/**
 * Recursively expand the variables view up to a given property.
 *
 * @param aOptions
 *        Options for view expansion:
 *        - rootVariable: start from the given scope/variable/property.
 *        - expandTo: string made up of property names you want to expand.
 *        For example: "body.firstChild.nextSibling" given |rootVariable:
 *        document|.
 *        - webconsole: a WebConsole instance. If this is not provided all
 *        property expand() calls will be considered sync. Things may fail!
 * @return object
 *         A promise that is resolved only when the last property in |expandTo|
 *         is found, and rejected otherwise. Resolution reason is always the
 *         last property - |nextSibling| in the example above. Rejection is
 *         always the last property that was found.
 */
function variablesViewExpandTo(aOptions)
{
  let root = aOptions.rootVariable;
  let expandTo = aOptions.expandTo.split(".");
  let jsterm = (aOptions.webconsole || {}).jsterm;
  let lastDeferred = promise.defer();

  function fetch(aProp)
  {
    if (!aProp.onexpand) {
      ok(false, "property " + aProp.name + " cannot be expanded: !onexpand");
      return promise.reject(aProp);
    }

    let deferred = promise.defer();

    if (aProp._fetched || !jsterm) {
      executeSoon(function() {
        deferred.resolve(aProp);
      });
    }
    else {
      jsterm.once("variablesview-fetched", function _onFetchProp() {
        executeSoon(() => deferred.resolve(aProp));
      });
    }

    aProp.expand();

    return deferred.promise;
  }

  function getNext(aProp)
  {
    let name = expandTo.shift();
    let newProp = aProp.get(name);

    if (expandTo.length > 0) {
      ok(newProp, "found property " + name);
      if (newProp) {
        fetch(newProp).then(getNext, fetchError);
      }
      else {
        lastDeferred.reject(aProp);
      }
    }
    else {
      if (newProp) {
        lastDeferred.resolve(newProp);
      }
      else {
        lastDeferred.reject(aProp);
      }
    }
  }

  function fetchError(aProp)
  {
    lastDeferred.reject(aProp);
  }

  if (!root._fetched) {
    fetch(root).then(getNext, fetchError);
  }
  else {
    getNext(root);
  }

  return lastDeferred.promise;
}


/**
 * Update the content of a property in the variables view.
 *
 * @param object aOptions
 *        Options for the property update:
 *        - property: the property you want to change.
 *        - field: string that tells what you want to change:
 *          - use "name" to change the property name,
 *          - or "value" to change the property value.
 *        - string: the new string to write into the field.
 *        - webconsole: reference to the Web Console instance we work with.
 * @return object
 *         A Promise object that is resolved once the property is updated.
 */
let updateVariablesViewProperty = Task.async(function* (aOptions) {
  let view = aOptions.property._variablesView;
  view.window.focus();
  aOptions.property.focus();

  switch (aOptions.field) {
    case "name":
      EventUtils.synthesizeKey("VK_RETURN", { shiftKey: true }, view.window);
      break;
    case "value":
      EventUtils.synthesizeKey("VK_RETURN", {}, view.window);
      break;
    default:
      throw new Error("options.field is incorrect");
  }

  let deferred = promise.defer();

  executeSoon(() => {
    EventUtils.synthesizeKey("A", { accelKey: true }, view.window);

    for (let c of aOptions.string) {
      EventUtils.synthesizeKey(c, {}, view.window);
    }

    if (aOptions.webconsole) {
      aOptions.webconsole.jsterm.once("variablesview-fetched").then((varView) => {
        deferred.resolve(varView);
      });
    }

    EventUtils.synthesizeKey("VK_RETURN", {}, view.window);

    if (!aOptions.webconsole) {
      executeSoon(() => {
        deferred.resolve(null);
      });
    }
  });

  return deferred.promise;
});

/**
 * Open the JavaScript debugger.
 *
 * @param object aOptions
 *        Options for opening the debugger:
 *        - tab: the tab you want to open the debugger for.
 * @return object
 *         A promise that is resolved once the debugger opens, or rejected if
 *         the open fails. The resolution callback is given one argument, an
 *         object that holds the following properties:
 *         - target: the Target object for the Tab.
 *         - toolbox: the Toolbox instance.
 *         - panel: the jsdebugger panel instance.
 *         - panelWin: the window object of the panel iframe.
 */
function openDebugger(aOptions = {})
{
  if (!aOptions.tab) {
    aOptions.tab = gBrowser.selectedTab;
  }

  let deferred = promise.defer();

  let target = TargetFactory.forTab(aOptions.tab);
  let toolbox = gDevTools.getToolbox(target);
  let dbgPanelAlreadyOpen = toolbox.getPanel("jsdebugger");

  gDevTools.showToolbox(target, "jsdebugger").then(function onSuccess(aToolbox) {
    let panel = aToolbox.getCurrentPanel();
    let panelWin = panel.panelWin;

    panel._view.Variables.lazyEmpty = false;

    let resolveObject = {
      target: target,
      toolbox: aToolbox,
      panel: panel,
      panelWin: panelWin,
    };

    if (dbgPanelAlreadyOpen) {
      deferred.resolve(resolveObject);
    }
    else {
      panelWin.once(panelWin.EVENTS.SOURCES_ADDED, () => {
        deferred.resolve(resolveObject);
      });
    }
  }, function onFailure(aReason) {
    console.debug("failed to open the toolbox for 'jsdebugger'", aReason);
    deferred.reject(aReason);
  });

  return deferred.promise;
}

/**
 * Wait for messages in the Web Console output.
 *
 * @param object aOptions
 *        Options for what you want to wait for:
 *        - webconsole: the webconsole instance you work with.
 *        - matchCondition: "any" or "all". Default: "all". The promise
 *        returned by this function resolves when all of the messages are
 *        matched, if the |matchCondition| is "all". If you set the condition to
 *        "any" then the promise is resolved by any message rule that matches,
 *        irrespective of order - waiting for messages stops whenever any rule
 *        matches.
 *        - messages: an array of objects that tells which messages to wait for.
 *        Properties:
 *            - text: string or RegExp to match the textContent of each new
 *            message.
 *            - noText: string or RegExp that must not match in the message
 *            textContent.
 *            - repeats: the number of message repeats, as displayed by the Web
 *            Console.
 *            - category: match message category. See CATEGORY_* constants at
 *            the top of this file.
 *            - severity: match message severity. See SEVERITY_* constants at
 *            the top of this file.
 *            - count: how many unique web console messages should be matched by
 *            this rule.
 *            - consoleTrace: boolean, set to |true| to match a console.trace()
 *            message. Optionally this can be an object of the form
 *            { file, fn, line } that can match the specified file, function
 *            and/or line number in the trace message.
 *            - consoleTime: string that matches a console.time() timer name.
 *            Provide this if you want to match a console.time() message.
 *            - consoleTimeEnd: same as above, but for console.timeEnd().
 *            - consoleDir: boolean, set to |true| to match a console.dir()
 *            message.
 *            - consoleGroup: boolean, set to |true| to match a console.group()
 *            message.
 *            - consoleTable: boolean, set to |true| to match a console.table()
 *            message.
 *            - longString: boolean, set to |true} to match long strings in the
 *            message.
 *            - collapsible: boolean, set to |true| to match messages that can
 *            be collapsed/expanded.
 *            - type: match messages that are instances of the given object. For
 *            example, you can point to Messages.NavigationMarker to match any
 *            such message.
 *            - objects: boolean, set to |true| if you expect inspectable
 *            objects in the message.
 *            - source: object of the shape { url, line }. This is used to
 *            match the source URL and line number of the error message or
 *            console API call.
 *            - stacktrace: array of objects of the form { file, fn, line } that
 *            can match frames in the stacktrace associated with the message.
 *            - groupDepth: number used to check the depth of the message in
 *            a group.
 *            - url: URL to match for network requests.
 * @return object
 *         A promise object is returned once the messages you want are found.
 *         The promise is resolved with the array of rule objects you give in
 *         the |messages| property. Each objects is the same as provided, with
 *         additional properties:
 *         - matched: a Set of web console messages that matched the rule.
 *         - clickableElements: a list of inspectable objects. This is available
 *         if any of the following properties are present in the rule:
 *         |consoleTrace| or |objects|.
 *         - longStrings: a list of long string ellipsis elements you can click
 *         in the message element, to expand a long string. This is available
 *         only if |longString| is present in the matching rule.
 */
function waitForMessages(aOptions)
{
  info("Waiting for messages...");

  gPendingOutputTest++;
  let webconsole = aOptions.webconsole;
  let rules = WebConsoleUtils.cloneObject(aOptions.messages, true);
  let rulesMatched = 0;
  let listenerAdded = false;
  let deferred = promise.defer();
  aOptions.matchCondition = aOptions.matchCondition || "all";

  function checkText(aRule, aText)
  {
    let result = false;
    if (Array.isArray(aRule)) {
      result = aRule.every((s) => checkText(s, aText));
    }
    else if (typeof aRule == "string") {
      result = aText.indexOf(aRule) > -1;
    }
    else if (aRule instanceof RegExp) {
      result = aRule.test(aText);
    }
    else {
      result = aRule == aText;
    }
    return result;
  }

  function checkConsoleTable(aRule, aElement)
  {
    let elemText = aElement.textContent;
    let table = aRule.consoleTable;

    if (!checkText("console.table():", elemText)) {
      return false;
    }

    aRule.category = CATEGORY_WEBDEV;
    aRule.severity = SEVERITY_LOG;
    aRule.type = Messages.ConsoleTable;

    return true;
  }

  function checkConsoleTrace(aRule, aElement)
  {
    let elemText = aElement.textContent;
    let trace = aRule.consoleTrace;

    if (!checkText("console.trace():", elemText)) {
      return false;
    }

    aRule.category = CATEGORY_WEBDEV;
    aRule.severity = SEVERITY_LOG;
    aRule.type = Messages.ConsoleTrace;

    if (!aRule.stacktrace && typeof trace == "object" && trace !== true) {
      if (Array.isArray(trace)) {
        aRule.stacktrace = trace;
      } else {
        aRule.stacktrace = [trace];
      }
    }

    return true;
  }

  function checkConsoleTime(aRule, aElement)
  {
    let elemText = aElement.textContent;
    let time = aRule.consoleTime;

    if (!checkText(time + ": timer started", elemText)) {
      return false;
    }

    aRule.category = CATEGORY_WEBDEV;
    aRule.severity = SEVERITY_LOG;

    return true;
  }

  function checkConsoleTimeEnd(aRule, aElement)
  {
    let elemText = aElement.textContent;
    let time = aRule.consoleTimeEnd;
    let regex = new RegExp(time + ": -?\\d+([,.]\\d+)?ms");

    if (!checkText(regex, elemText)) {
      return false;
    }

    aRule.category = CATEGORY_WEBDEV;
    aRule.severity = SEVERITY_LOG;

    return true;
  }

  function checkConsoleDir(aRule, aElement)
  {
    if (!aElement.classList.contains("inlined-variables-view")) {
      return false;
    }

    let elemText = aElement.textContent;
    if (!checkText(aRule.consoleDir, elemText)) {
      return false;
    }

    let iframe = aElement.querySelector("iframe");
    if (!iframe) {
      ok(false, "console.dir message has no iframe");
      return false;
    }

    return true;
  }

  function checkConsoleGroup(aRule, aElement)
  {
    if (!isNaN(parseInt(aRule.consoleGroup))) {
      aRule.groupDepth = aRule.consoleGroup;
    }
    aRule.category = CATEGORY_WEBDEV;
    aRule.severity = SEVERITY_LOG;

    return true;
  }

  function checkSource(aRule, aElement)
  {
    let location = aElement.querySelector(".message-location");
    if (!location) {
      return false;
    }

    if (!checkText(aRule.source.url, location.getAttribute("title"))) {
      return false;
    }

    if ("line" in aRule.source && location.sourceLine != aRule.source.line) {
      return false;
    }

    return true;
  }

  function checkCollapsible(aRule, aElement)
  {
    let msg = aElement._messageObject;
    if (!msg || !!msg.collapsible != aRule.collapsible) {
      return false;
    }

    return true;
  }

  function checkStacktrace(aRule, aElement)
  {
    let stack = aRule.stacktrace;
    let frames = aElement.querySelectorAll(".stacktrace > li");
    if (!frames.length) {
      return false;
    }

    for (let i = 0; i < stack.length; i++) {
      let frame = frames[i];
      let expected = stack[i];
      if (!frame) {
        ok(false, "expected frame #" + i + " but didnt find it");
        return false;
      }

      if (expected.file) {
        let file = frame.querySelector(".message-location").title;
        if (!checkText(expected.file, file)) {
          ok(false, "frame #" + i + " does not match file name: " +
                    expected.file);
          displayErrorContext(aRule, aElement);
          return false;
        }
      }

      if (expected.fn) {
        let fn = frame.querySelector(".function").textContent;
        if (!checkText(expected.fn, fn)) {
          ok(false, "frame #" + i + " does not match the function name: " +
                    expected.fn);
          displayErrorContext(aRule, aElement);
          return false;
        }
      }

      if (expected.line) {
        let line = frame.querySelector(".message-location").sourceLine;
        if (!checkText(expected.line, line)) {
          ok(false, "frame #" + i + " does not match the line number: " +
                    expected.line);
          displayErrorContext(aRule, aElement);
          return false;
        }
      }
    }

    return true;
  }

  function hasXhrLabel(aElement) {
    let xhr = aElement.querySelector('.xhr');
    if (!xhr) {
      return false;
    }
    return true;
  }

  function checkMessage(aRule, aElement)
  {
    let elemText = aElement.textContent;

    if (aRule.text && !checkText(aRule.text, elemText)) {
      return false;
    }

    if (aRule.noText && checkText(aRule.noText, elemText)) {
      return false;
    }

    if (aRule.consoleTable && !checkConsoleTable(aRule, aElement)) {
      return false;
    }

    if (aRule.consoleTrace && !checkConsoleTrace(aRule, aElement)) {
      return false;
    }

    if (aRule.consoleTime && !checkConsoleTime(aRule, aElement)) {
      return false;
    }

    if (aRule.consoleTimeEnd && !checkConsoleTimeEnd(aRule, aElement)) {
      return false;
    }

    if (aRule.consoleDir && !checkConsoleDir(aRule, aElement)) {
      return false;
    }

    if (aRule.consoleGroup && !checkConsoleGroup(aRule, aElement)) {
      return false;
    }

    if (aRule.source && !checkSource(aRule, aElement)) {
      return false;
    }

    if ("collapsible" in aRule && !checkCollapsible(aRule, aElement)) {
      return false;
    }

    if (aRule.isXhr && !hasXhrLabel(aElement)) {
      return false;
    }

    if (!aRule.isXhr && hasXhrLabel(aElement)) {
      return false;
    }

    let partialMatch = !!(aRule.consoleTrace || aRule.consoleTime ||
                          aRule.consoleTimeEnd);

    // The rule tries to match the newer types of messages, based on their
    // object constructor.
    if (aRule.type) {
      if (!aElement._messageObject ||
          !(aElement._messageObject instanceof aRule.type)) {
        if (partialMatch) {
          ok(false, "message type for rule: " + displayRule(aRule));
          displayErrorContext(aRule, aElement);
        }
        return false;
      }
      partialMatch = true;
    }

    if ("category" in aRule && aElement.category != aRule.category) {
      if (partialMatch) {
        is(aElement.category, aRule.category,
           "message category for rule: " + displayRule(aRule));
        displayErrorContext(aRule, aElement);
      }
      return false;
    }

    if ("severity" in aRule && aElement.severity != aRule.severity) {
      if (partialMatch) {
        is(aElement.severity, aRule.severity,
           "message severity for rule: " + displayRule(aRule));
        displayErrorContext(aRule, aElement);
      }
      return false;
    }

    if (aRule.text) {
      partialMatch = true;
    }

    if (aRule.stacktrace && !checkStacktrace(aRule, aElement)) {
      if (partialMatch) {
        ok(false, "failed to match stacktrace for rule: " + displayRule(aRule));
        displayErrorContext(aRule, aElement);
      }
      return false;
    }

    if (aRule.category == CATEGORY_NETWORK && "url" in aRule &&
        !checkText(aRule.url, aElement.url)) {
      return false;
    }

    if ("repeats" in aRule) {
      let repeats = aElement.querySelector(".message-repeats");
      if (!repeats || repeats.getAttribute("value") != aRule.repeats) {
        return false;
      }
    }

    if ("groupDepth" in aRule) {
      let indentNode = aElement.querySelector(".indent");
      let indent = (GROUP_INDENT * aRule.groupDepth)  + "px";
      if (!indentNode || indentNode.style.width != indent) {
        is(indentNode.style.width, indent,
           "group depth check failed for message rule: " + displayRule(aRule));
        return false;
      }
    }

    if ("longString" in aRule) {
      let longStrings = aElement.querySelectorAll(".longStringEllipsis");
      if (aRule.longString != !!longStrings[0]) {
        if (partialMatch) {
          is(!!longStrings[0], aRule.longString,
             "long string existence check failed for message rule: " +
             displayRule(aRule));
          displayErrorContext(aRule, aElement);
        }
        return false;
      }
      aRule.longStrings = longStrings;
    }

    if ("objects" in aRule) {
      let clickables = aElement.querySelectorAll(".message-body a");
      if (aRule.objects != !!clickables[0]) {
        if (partialMatch) {
          is(!!clickables[0], aRule.objects,
             "objects existence check failed for message rule: " +
             displayRule(aRule));
          displayErrorContext(aRule, aElement);
        }
        return false;
      }
      aRule.clickableElements = clickables;
    }

    let count = aRule.count || 1;
    if (!aRule.matched) {
      aRule.matched = new Set();
    }
    aRule.matched.add(aElement);

    return aRule.matched.size == count;
  }

  function onMessagesAdded(aEvent, aNewMessages)
  {
    for (let msg of aNewMessages) {
      let elem = msg.node;
      let location = elem.querySelector(".message-location");
      if (location) {
        let url = location.title;
        // Prevent recursion with the browser console and any potential
        // messages coming from head.js.
        if (url.indexOf("browser/devtools/webconsole/test/head.js") != -1) {
          continue;
        }
      }

      for (let rule of rules) {
        if (rule._ruleMatched) {
          continue;
        }

        let matched = checkMessage(rule, elem);
        if (matched) {
          rule._ruleMatched = true;
          rulesMatched++;
          ok(1, "matched rule: " + displayRule(rule));
          if (maybeDone()) {
            return;
          }
        }
      }
    }
  }

  function allRulesMatched()
  {
    return aOptions.matchCondition == "all" && rulesMatched == rules.length ||
           aOptions.matchCondition == "any" && rulesMatched > 0;
  }

  function maybeDone()
  {
    if (allRulesMatched()) {
      if (listenerAdded) {
        webconsole.ui.off("new-messages", onMessagesAdded);
      }
      gPendingOutputTest--;
      deferred.resolve(rules);
      return true;
    }
    return false;
  }

  function testCleanup() {
    if (allRulesMatched()) {
      return;
    }

    if (webconsole.ui) {
      webconsole.ui.off("new-messages", onMessagesAdded);
    }

    for (let rule of rules) {
      if (!rule._ruleMatched) {
        ok(false, "failed to match rule: " + displayRule(rule));
      }
    }
  }

  function displayRule(aRule)
  {
    return aRule.name || aRule.text;
  }

  function displayErrorContext(aRule, aElement)
  {
    console.log("error occured during rule " + displayRule(aRule));
    console.log("while checking the following message");
    dumpMessageElement(aElement);
  }

  executeSoon(() => {

    let messages = [];
    for (let elem of webconsole.outputNode.childNodes) {
      messages.push({
        node: elem,
        update: false,
      });
    }

    onMessagesAdded("new-messages", messages);

    if (!allRulesMatched()) {
      listenerAdded = true;
      registerCleanupFunction(testCleanup);
      webconsole.ui.on("new-messages", onMessagesAdded);
    }
  });

  return deferred.promise;
}

function whenDelayedStartupFinished(aWindow, aCallback)
{
  Services.obs.addObserver(function observer(aSubject, aTopic) {
    if (aWindow == aSubject) {
      Services.obs.removeObserver(observer, aTopic);
      executeSoon(aCallback);
    }
  }, "browser-delayed-startup-finished", false);
}

/**
 * Check the web console output for the given inputs. Each input is checked for
 * the expected JS eval result, the result of calling print(), the result of
 * console.log(). The JS eval result is also checked if it opens the variables
 * view on click.
 *
 * @param object hud
 *        The web console instance to work with.
 * @param array inputTests
 *        An array of input tests. An input test element is an object. Each
 *        object has the following properties:
 *        - input: string, JS input value to execute.
 *
 *        - output: string|RegExp, expected JS eval result.
 *
 *        - inspectable: boolean, when true, the test runner expects the JS eval
 *        result is an object that can be clicked for inspection.
 *
 *        - noClick: boolean, when true, the test runner does not click the JS
 *        eval result. Some objects, like |window|, have a lot of properties and
 *        opening vview for them is very slow (they can cause timeouts in debug
 *        builds).
 *
 *        - printOutput: string|RegExp, optional, expected output for
 *        |print(input)|. If this is not provided, printOutput = output.
 *
 *        - variablesViewLabel: string|RegExp, optional, the expected variables
 *        view label when the object is inspected. If this is not provided, then
 *        |output| is used.
 *
 *        - inspectorIcon: boolean, when true, the test runner expects the
 *        result widget to contain an inspectorIcon element (className
 *        open-inspector).
 *
 *        - expectedTab: string, optional, the full URL of the new tab which must
 *        open. If this is not provided, any new tabs that open will cause a test
 *        failure.
 */
function checkOutputForInputs(hud, inputTests)
{
  let container = gBrowser.tabContainer;

  function* runner()
  {
    for (let [i, entry] of inputTests.entries()) {
      info("checkInput(" + i + "): " + entry.input);
      yield checkInput(entry);
    }
    container = null;
  }

  function* checkInput(entry)
  {
    yield checkConsoleLog(entry);
    yield checkPrintOutput(entry);
    yield checkJSEval(entry);
  }

  function* checkConsoleLog(entry)
  {
    info("Logging: " + entry.input);
    hud.jsterm.clearOutput();
    hud.jsterm.execute("console.log(" + entry.input + ")");

    let [result] = yield waitForMessages({
      webconsole: hud,
      messages: [{
        name: "console.log() output: " + entry.output,
        text: entry.output,
        category: CATEGORY_WEBDEV,
        severity: SEVERITY_LOG,
      }],
    });

    if (typeof entry.inspectorIcon == "boolean") {
      let msg = [...result.matched][0];
      yield checkLinkToInspector(entry, msg);
    }
  }

  function checkPrintOutput(entry)
  {
    info("Printing: " + entry.input);
    hud.jsterm.clearOutput();
    hud.jsterm.execute("print(" + entry.input + ")");

    let printOutput = entry.printOutput || entry.output;

    return waitForMessages({
      webconsole: hud,
      messages: [{
        name: "print() output: " + printOutput,
        text: printOutput,
        category: CATEGORY_OUTPUT,
      }],
    });
  }

  function* checkJSEval(entry)
  {
    info("Evaluating: " + entry.input);
    hud.jsterm.clearOutput();
    hud.jsterm.execute(entry.input);

    let [result] = yield waitForMessages({
      webconsole: hud,
      messages: [{
        name: "JS eval output: " + entry.output,
        text: entry.output,
        category: CATEGORY_OUTPUT,
      }],
    });

    let msg = [...result.matched][0];
    if (!entry.noClick) {
      yield checkObjectClick(entry, msg);
    }
    if (typeof entry.inspectorIcon == "boolean") {
      yield checkLinkToInspector(entry, msg);
    }
  }

  function* checkObjectClick(entry, msg)
  {
    info("Clicking: " + entry.input);
    let body = msg.querySelector(".message-body a") ||
               msg.querySelector(".message-body");
    ok(body, "the message body");

    let deferredVariablesView = promise.defer();
    entry._onVariablesViewOpen = onVariablesViewOpen.bind(null, entry, deferredVariablesView);
    hud.jsterm.on("variablesview-open", entry._onVariablesViewOpen);

    let deferredTab = promise.defer();
    entry._onTabOpen = onTabOpen.bind(null, entry, deferredTab);
    container.addEventListener("TabOpen", entry._onTabOpen, true);

    body.scrollIntoView();
    EventUtils.synthesizeMouse(body, 2, 2, {}, hud.iframeWindow);

    if (entry.inspectable) {
      info("message body tagName '" + body.tagName +  "' className '" + body.className + "'");
      yield deferredVariablesView.promise;
    } else {
      hud.jsterm.off("variablesview-open", entry._onVariablesView);
      entry._onVariablesView = null;
    }

    if (entry.expectedTab) {
      yield deferredTab.promise;
    } else {
      container.removeEventListener("TabOpen", entry._onTabOpen, true);
      entry._onTabOpen = null;
    }

    yield promise.resolve(null);
  }

  function checkLinkToInspector(entry, msg)
  {
    info("Checking Inspector Link: " + entry.input);
    let elementNodeWidget = [...msg._messageObject.widgets][0];
    if (!elementNodeWidget) {
      ok(!entry.inspectorIcon, "The message has no ElementNode widget");
      return;
    }

    return elementNodeWidget.linkToInspector().then(() => {
      // linkToInspector resolved, check for the .open-inspector element
      if (entry.inspectorIcon) {
        ok(msg.querySelectorAll(".open-inspector").length,
          "The ElementNode widget is linked to the inspector");
      } else {
        ok(!msg.querySelectorAll(".open-inspector").length,
          "The ElementNode widget isn't linked to the inspector");
      }
    }, () => {
      // linkToInspector promise rejected, node not linked to inspector
      ok(!entry.inspectorIcon, "The ElementNode widget isn't linked to the inspector");
    });
  }

  function onVariablesViewOpen(entry, {resolve, reject}, event, view, options)
  {
    info("Variables view opened: " + entry.input);
    let label = entry.variablesViewLabel || entry.output;
    if (typeof label == "string" && options.label != label) {
      return;
    }
    if (label instanceof RegExp && !label.test(options.label)) {
      return;
    }

    hud.jsterm.off("variablesview-open", entry._onVariablesViewOpen);
    entry._onVariablesViewOpen = null;
    ok(entry.inspectable, "variables view was shown");

    resolve(null);
  }

  function onTabOpen(entry, {resolve, reject}, event)
  {
    container.removeEventListener("TabOpen", entry._onTabOpen, true);
    entry._onTabOpen = null;

    let tab = event.target;
    let browser = gBrowser.getBrowserForTab(tab);
    loadBrowser(browser).then(() => {
      let uri = content.location.href;
      ok(entry.expectedTab && entry.expectedTab == uri,
        "opened tab '" + uri +  "', expected tab '" + entry.expectedTab + "'");
      return closeTab(tab);
    }).then(resolve, reject);
  }

  return Task.spawn(runner);
}

/**
 * Wait for eventName on target.
 * @param {Object} target An observable object that either supports on/off or
 * addEventListener/removeEventListener
 * @param {String} eventName
 * @param {Boolean} useCapture Optional, for addEventListener/removeEventListener
 * @return A promise that resolves when the event has been handled
 */
function once(target, eventName, useCapture=false) {
  info("Waiting for event: '" + eventName + "' on " + target + ".");

  let deferred = promise.defer();

  for (let [add, remove] of [
    ["addEventListener", "removeEventListener"],
    ["addListener", "removeListener"],
    ["on", "off"]
  ]) {
    if ((add in target) && (remove in target)) {
      target[add](eventName, function onEvent(...aArgs) {
        target[remove](eventName, onEvent, useCapture);
        deferred.resolve.apply(deferred, aArgs);
      }, useCapture);
      break;
    }
  }

  return deferred.promise;
}

function getSourceActor(aSources, aURL) {
  let item = aSources.getItemForAttachment(a => a.source.url === aURL);
  return item && item.value;
}
