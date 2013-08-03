/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let WebConsoleUtils, gDevTools, TargetFactory, console, promise;

(() => {
  gDevTools = Cu.import("resource:///modules/devtools/gDevTools.jsm", {}).gDevTools;
  console = Cu.import("resource://gre/modules/devtools/Console.jsm", {}).console;
  promise = Cu.import("resource://gre/modules/commonjs/sdk/core/promise.js", {}).Promise;

  let tools = Cu.import("resource://gre/modules/devtools/Loader.jsm", {}).devtools;
  let utils = tools.require("devtools/toolkit/webconsole/utils");
  TargetFactory = tools.TargetFactory;
  WebConsoleUtils = utils.Utils;
})();
// promise._reportErrors = true; // please never leave me.

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
    let hud = toolbox.getCurrentPanel().hud;
    hud.jsterm._lazyVariablesView = false;
    aCallback(hud);
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
      toolbox.destroy().then(aCallback.bind(null, hudId)).then(null, console.debug);
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
 */
function waitForContextMenu(aPopup, aButton, aOnShown, aOnHidden)
{
  function onPopupShown() {
    info("onPopupShown");
    aPopup.removeEventListener("popupshown", onPopupShown);

    aOnShown();

    // Use executeSoon() to get out of the popupshown event.
    aPopup.addEventListener("popuphidden", onPopupHidden);
    executeSoon(() => aPopup.hidePopup());
  }
  function onPopupHidden() {
    info("onPopupHidden");
    aPopup.removeEventListener("popuphidden", onPopupHidden);
    aOnHidden();
  }

  aPopup.addEventListener("popupshown", onPopupShown);

  info("wait for the context menu to open");
  let eventDetails = { type: "contextmenu", button: 2};
  EventUtils.synthesizeMouse(aButton, 2, 2, eventDetails,
                             aButton.ownerDocument.defaultView);
}

/**
 * Dump the output of all open Web Consoles - used only for debugging purposes.
 */
function dumpConsoles()
{
  if (gPendingOutputTest) {
    console.log("dumpConsoles start");
    for (let hud of HUDService.consoles) {
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
  let text = getMessageElementText(aMessage);
  let repeats = aMessage.querySelector(".webconsole-msg-repeat");
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

function finishTest()
{
  browser = hudId = hud = filterBox = outputNode = cs = null;

  dumpConsoles();

  let browserConsole = HUDService.getBrowserConsole();
  if (browserConsole) {
    if (browserConsole.jsterm) {
      browserConsole.jsterm.clearOutput(true);
    }
    HUDService.toggleBrowserConsole().then(finishTest);
    return;
  }

  let hud = HUDService.getHudByWindow(content);
  if (!hud) {
    finish();
    return;
  }

  if (hud.jsterm) {
    hud.jsterm.clearOutput(true);
  }

  closeConsole(hud.target.tab, finish);

  hud = null;
}

function tearDown()
{
  dumpConsoles();

  if (HUDService.getBrowserConsole()) {
    HUDService.toggleBrowserConsole();
  }

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
    for (let [id, prop] in aVar) {
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
 *        - callback: function to invoke after the property is updated.
 */
function updateVariablesViewProperty(aOptions)
{
  let view = aOptions.property._variablesView;
  view.window.focus();
  aOptions.property.focus();

  switch (aOptions.field) {
    case "name":
      EventUtils.synthesizeKey("VK_ENTER", { shiftKey: true }, view.window);
      break;
    case "value":
      EventUtils.synthesizeKey("VK_ENTER", {}, view.window);
      break;
    default:
      throw new Error("options.field is incorrect");
      return;
  }

  executeSoon(() => {
    EventUtils.synthesizeKey("A", { accelKey: true }, view.window);

    for (let c of aOptions.string) {
      EventUtils.synthesizeKey(c, {}, gVariablesView.window);
    }

    if (aOptions.webconsole) {
      aOptions.webconsole.jsterm.once("variablesview-fetched", aOptions.callback);
    }

    EventUtils.synthesizeKey("VK_ENTER", {}, view.window);

    if (!aOptions.webconsole) {
      executeSoon(aOptions.callback);
    }
  });
}

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
    panel._view.Variables.lazyAppend = false;

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
      panelWin.addEventListener("Debugger:AfterSourcesAdded",
        function onAfterSourcesAdded() {
          panelWin.removeEventListener("Debugger:AfterSourcesAdded",
                                       onAfterSourcesAdded);
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
 * Get the full text displayed by a Web Console message.
 *
 * @param nsIDOMElement aElement
 *        The message element from the Web Console output.
 * @return string
 *         The full text displayed by the given message element.
 */
function getMessageElementText(aElement)
{
  let text = aElement.textContent;
  let labels = aElement.querySelectorAll("label");
  for (let label of labels) {
    text += " " + label.getAttribute("value");
  }
  return text;
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
 *            - longString: boolean, set to |true} to match long strings in the
 *            message.
 *            - objects: boolean, set to |true| if you expect inspectable
 *            objects in the message.
 *            - source: object that can hold one property: url. This is used to
 *            match the source URL of the message.
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
  gPendingOutputTest++;
  let webconsole = aOptions.webconsole;
  let rules = WebConsoleUtils.cloneObject(aOptions.messages, true);
  let rulesMatched = 0;
  let listenerAdded = false;
  let deferred = promise.defer();
  aOptions.matchCondition = aOptions.matchCondition || "all";

  function checkText(aRule, aText)
  {
    let result;
    if (typeof aRule == "string") {
      result = aText.indexOf(aRule) > -1;
    }
    else if (aRule instanceof RegExp) {
      result = aRule.test(aText);
    }
    return result;
  }

  function checkConsoleTrace(aRule, aElement)
  {
    let elemText = getMessageElementText(aElement);
    let trace = aRule.consoleTrace;

    if (!checkText("Stack trace from ", elemText)) {
      return false;
    }

    let clickable = aElement.querySelector(".hud-clickable");
    if (!clickable) {
      ok(false, "console.trace() message is missing .hud-clickable");
      displayErrorContext(aRule, aElement);
      return false;
    }
    aRule.clickableElements = [clickable];

    if (trace.file &&
        !checkText("from " + trace.file + ", ", elemText)) {
      ok(false, "console.trace() message is missing the file name: " +
                trace.file);
      displayErrorContext(aRule, aElement);
      return false;
    }

    if (trace.fn &&
        !checkText(", function " + trace.fn + ", ", elemText)) {
      ok(false, "console.trace() message is missing the function name: " +
                trace.fn);
      displayErrorContext(aRule, aElement);
      return false;
    }

    if (trace.line &&
        !checkText(", line " + trace.line + ".", elemText)) {
      ok(false, "console.trace() message is missing the line number: " +
                trace.line);
      displayErrorContext(aRule, aElement);
      return false;
    }

    aRule.category = CATEGORY_WEBDEV;
    aRule.severity = SEVERITY_LOG;

    return true;
  }

  function checkConsoleTime(aRule, aElement)
  {
    let elemText = getMessageElementText(aElement);
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
    let elemText = getMessageElementText(aElement);
    let time = aRule.consoleTimeEnd;
    let regex = new RegExp(time + ": -?\\d+ms");

    if (!checkText(regex, elemText)) {
      return false;
    }

    aRule.category = CATEGORY_WEBDEV;
    aRule.severity = SEVERITY_LOG;

    return true;
  }

  function checkConsoleDir(aRule, aElement)
  {
    if (!aElement.classList.contains("webconsole-msg-inspector")) {
      return false;
    }

    let elemText = getMessageElementText(aElement);
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

  function checkSource(aRule, aElement)
  {
    let location = aElement.querySelector(".webconsole-location");
    if (!location) {
      return false;
    }

    return checkText(aRule.source.url, location.getAttribute("title"));
  }

  function checkMessage(aRule, aElement)
  {
    let elemText = getMessageElementText(aElement);

    if (aRule.text && !checkText(aRule.text, elemText)) {
      return false;
    }

    if (aRule.noText && checkText(aRule.noText, elemText)) {
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

    if (aRule.source && !checkSource(aRule, aElement)) {
      return false;
    }

    let partialMatch = !!(aRule.consoleTrace || aRule.consoleTime ||
                          aRule.consoleTimeEnd);

    if (aRule.category && aElement.category != aRule.category) {
      if (partialMatch) {
        is(aElement.category, aRule.category,
           "message category for rule: " + displayRule(aRule));
        displayErrorContext(aRule, aElement);
      }
      return false;
    }

    if (aRule.severity && aElement.severity != aRule.severity) {
      if (partialMatch) {
        is(aElement.severity, aRule.severity,
           "message severity for rule: " + displayRule(aRule));
        displayErrorContext(aRule, aElement);
      }
      return false;
    }

    if (aRule.repeats) {
      let repeats = aElement.querySelector(".webconsole-msg-repeat");
      if (!repeats || repeats.getAttribute("value") != aRule.repeats) {
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
      let clickables = aElement.querySelectorAll(".hud-clickable");
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

  function onMessagesAdded(aEvent, aNewElements)
  {
    for (let elem of aNewElements) {
      let location = elem.querySelector(".webconsole-location");
      if (location) {
        let url = location.getAttribute("title");
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
        webconsole.ui.off("messages-added", onMessagesAdded);
        webconsole.ui.off("messages-updated", onMessagesAdded);
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
      webconsole.ui.off("messages-added", onMessagesAdded);
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
    onMessagesAdded("messages-added", webconsole.outputNode.childNodes);
    if (!allRulesMatched()) {
      listenerAdded = true;
      registerCleanupFunction(testCleanup);
      webconsole.ui.on("messages-added", onMessagesAdded);
      webconsole.ui.on("messages-updated", onMessagesAdded);
    }
  });

  return deferred.promise;
}


/**
 * Scroll the Web Console output to the given node.
 *
 * @param nsIDOMNode aNode
 *        The node to scroll to.
 */
function scrollOutputToNode(aNode)
{
  let richListBoxNode = aNode.parentNode;
  while (richListBoxNode.tagName != "richlistbox") {
    richListBoxNode = richListBoxNode.parentNode;
  }

  let boxObject = richListBoxNode.scrollBoxObject;
  let nsIScrollBoxObject = boxObject.QueryInterface(Ci.nsIScrollBoxObject);
  nsIScrollBoxObject.ensureElementIsVisible(aNode);
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
