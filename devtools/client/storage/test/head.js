/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* eslint no-unused-vars: [2, {"vars": "local"}] */

var { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
var { TargetFactory } = require("devtools/client/framework/target");
var promise = require("promise");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");

const SPLIT_CONSOLE_PREF = "devtools.toolbox.splitconsoleEnabled";
const STORAGE_PREF = "devtools.storage.enabled";
const DUMPEMIT_PREF = "devtools.dump.emit";
const DEBUGGERLOG_PREF = "devtools.debugger.log";
// Allows Cache API to be working on usage `http` test page
const CACHES_ON_HTTP_PREF = "dom.caches.testing.enabled";
const PATH = "browser/devtools/client/storage/test/";
const MAIN_DOMAIN = "http://test1.example.org/" + PATH;
const ALT_DOMAIN = "http://sectest1.example.org/" + PATH;
const ALT_DOMAIN_SECURED = "https://sectest1.example.org:443/" + PATH;

waitForExplicitFinish();

var gToolbox, gPanelWindow, gWindow, gUI;

// Services.prefs.setBoolPref(DUMPEMIT_PREF, true);
// Services.prefs.setBoolPref(DEBUGGERLOG_PREF, true);

Services.prefs.setBoolPref(STORAGE_PREF, true);
Services.prefs.setBoolPref(CACHES_ON_HTTP_PREF, true);
DevToolsUtils.testing = true;
registerCleanupFunction(() => {
  gToolbox = gPanelWindow = gWindow = gUI = null;
  Services.prefs.clearUserPref(STORAGE_PREF);
  Services.prefs.clearUserPref(SPLIT_CONSOLE_PREF);
  Services.prefs.clearUserPref(DUMPEMIT_PREF);
  Services.prefs.clearUserPref(DEBUGGERLOG_PREF);
  Services.prefs.clearUserPref(CACHES_ON_HTTP_PREF);
  DevToolsUtils.testing = false;
  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

/**
 * Add a new test tab in the browser and load the given url.
 *
 * @param {String} url The url to be loaded in the new tab
 *
 * @return a promise that resolves to the content window when the url is loaded
 */
function addTab(url) {
  info("Adding a new tab with URL: '" + url + "'");
  let def = promise.defer();

  // Bug 921935 should bring waitForFocus() support to e10s, which would
  // probably cover the case of the test losing focus when the page is loading.
  // For now, we just make sure the window is focused.
  window.focus();

  let tab = window.gBrowser.selectedTab = window.gBrowser.addTab(url);
  let linkedBrowser = tab.linkedBrowser;

  linkedBrowser.addEventListener("load", function onload(event) {
    if (event.originalTarget.location.href != url) {
      return;
    }
    linkedBrowser.removeEventListener("load", onload, true);
    info("URL '" + url + "' loading complete");
    def.resolve(tab.linkedBrowser.contentWindow);
  }, true);

  return def.promise;
}

/**
 * This generator function opens the given url in a new tab, then sets up the
 * page by waiting for all cookies, indexedDB items etc. to be created; Then
 * opens the storage inspector and waits for the storage tree and table to be
 * populated.
 *
 * @param url {String} The url to be opened in the new tab
 *
 * @return {Promise} A promise that resolves after storage inspector is ready
 */
function* openTabAndSetupStorage(url) {
  let content = yield addTab(url);

  gWindow = content.wrappedJSObject;

  // Setup the async storages in main window and for all its iframes
  yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
    /**
     * Get all windows including frames recursively.
     *
     * @param {Window} [baseWindow]
     *        The base window at which to start looking for child windows
     *        (optional).
     * @return {Set}
     *         A set of windows.
     */
    function getAllWindows(baseWindow) {
      let windows = new Set();

      let _getAllWindows = function(win) {
        windows.add(win.wrappedJSObject);

        for (let i = 0; i < win.length; i++) {
          _getAllWindows(win[i]);
        }
      };
      _getAllWindows(baseWindow);

      return windows;
    }

    let windows = getAllWindows(content);
    for (let win of windows) {
      if (win.setup) {
        yield win.setup();
      }
    }
  });

  // open storage inspector
  return yield openStoragePanel();
}

/**
 * Open the toolbox, with the storage tool visible.
 *
 * @param cb {Function} Optional callback, if you don't want to use the returned
 *                      promise
 *
 * @return {Promise} a promise that resolves when the storage inspector is ready
 */
var openStoragePanel = Task.async(function*(cb) {
  info("Opening the storage inspector");
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  let storage, toolbox;

  // Checking if the toolbox and the storage are already loaded
  // The storage-updated event should only be waited for if the storage
  // isn't loaded yet
  toolbox = gDevTools.getToolbox(target);
  if (toolbox) {
    storage = toolbox.getPanel("storage");
    if (storage) {
      gPanelWindow = storage.panelWindow;
      gUI = storage.UI;
      gToolbox = toolbox;
      info("Toolbox and storage already open");
      if (cb) {
        return cb(storage, toolbox);
      }

      return {
        toolbox: toolbox,
        storage: storage
      };
    }
  }

  info("Opening the toolbox");
  toolbox = yield gDevTools.showToolbox(target, "storage");
  storage = toolbox.getPanel("storage");
  gPanelWindow = storage.panelWindow;
  gUI = storage.UI;
  gToolbox = toolbox;

  // The table animation flash causes some timeouts on Linux debug tests,
  // so we disable it
  gUI.animationsEnabled = false;

  info("Waiting for the stores to update");
  yield gUI.once("store-objects-updated");

  yield waitForToolboxFrameFocus(toolbox);

  if (cb) {
    return cb(storage, toolbox);
  }

  return {
    toolbox: toolbox,
    storage: storage
  };
});

/**
 * Wait for the toolbox frame to receive focus after it loads
 *
 * @param toolbox {Toolbox}
 *
 * @return a promise that resolves when focus has been received
 */
function waitForToolboxFrameFocus(toolbox) {
  info("Making sure that the toolbox's frame is focused");
  let def = promise.defer();
  let win = toolbox.frame.contentWindow;
  waitForFocus(def.resolve, win);
  return def.promise;
}

/**
 * Forces GC, CC and Shrinking GC to get rid of disconnected docshells and
 * windows.
 */
function forceCollections() {
  Cu.forceGC();
  Cu.forceCC();
  Cu.forceShrinkingGC();
}

/**
 * Cleans up and finishes the test
 */
function* finishTests() {
  // Bug 1233497 makes it so that we can no longer yield CPOWs from Tasks.
  // We work around this by calling clear() via a ContentTask instead.
  yield ContentTask.spawn(gBrowser.selectedBrowser, null, function*() {
    /**
     * Get all windows including frames recursively.
     *
     * @param {Window} [baseWindow]
     *        The base window at which to start looking for child windows
     *        (optional).
     * @return {Set}
     *         A set of windows.
     */
    function getAllWindows(baseWindow) {
      let windows = new Set();

      let _getAllWindows = function(win) {
        windows.add(win.wrappedJSObject);

        for (let i = 0; i < win.length; i++) {
          _getAllWindows(win[i]);
        }
      };
      _getAllWindows(baseWindow);

      return windows;
    }

    let windows = getAllWindows(content);
    for (let win of windows) {
      if (win.clear) {
        yield win.clear();
      }
    }
  });

  forceCollections();
  finish();
}

// Sends a click event on the passed DOM node in an async manner
function* click(node) {
  let def = promise.defer();

  node.scrollIntoView();

  // We need setTimeout here to allow any scrolling to complete before clicking
  // the node.
  setTimeout(() => {
    node.click();
    def.resolve();
  }, 200);

  return def;
}

/**
 * Recursively expand the variables view up to a given property.
 *
 * @param options
 *        Options for view expansion:
 *        - rootVariable: start from the given scope/variable/property.
 *        - expandTo: string made up of property names you want to expand.
 *        For example: "body.firstChild.nextSibling" given |rootVariable:
 *        document|.
 * @return object
 *         A promise that is resolved only when the last property in |expandTo|
 *         is found, and rejected otherwise. Resolution reason is always the
 *         last property - |nextSibling| in the example above. Rejection is
 *         always the last property that was found.
 */
function variablesViewExpandTo(options) {
  let root = options.rootVariable;
  let expandTo = options.expandTo.split(".");
  let lastDeferred = promise.defer();

  function getNext(prop) {
    let name = expandTo.shift();
    let newProp = prop.get(name);

    if (expandTo.length > 0) {
      ok(newProp, "found property " + name);
      if (newProp && newProp.expand) {
        newProp.expand();
        getNext(newProp);
      } else {
        lastDeferred.reject(prop);
      }
    } else if (newProp) {
      lastDeferred.resolve(newProp);
    } else {
      lastDeferred.reject(prop);
    }
  }

  if (root && root.expand) {
    root.expand();
    getNext(root);
  } else {
    lastDeferred.resolve(root);
  }

  return lastDeferred.promise;
}

/**
 * Find variables or properties in a VariablesView instance.
 *
 * @param array ruleArray
 *        The array of rules you want to match. Each rule is an object with:
 *        - name (string|regexp): property name to match.
 *        - value (string|regexp): property value to match.
 *        - dontMatch (boolean): make sure the rule doesn't match any property.
 * @param boolean parsed
 *        true if we want to test the rules in the parse value section of the
 *        storage sidebar
 * @return object
 *         A promise object that is resolved when all the rules complete
 *         matching. The resolved callback is given an array of all the rules
 *         you wanted to check. Each rule has a new property: |matchedProp|
 *         which holds a reference to the Property object instance from the
 *         VariablesView. If the rule did not match, then |matchedProp| is
 *         undefined.
 */
function findVariableViewProperties(ruleArray, parsed) {
  // Initialize the search.
  function init() {
    // If parsed is true, we are checking rules in the parsed value section of
    // the storage sidebar. That scope uses a blank variable as a placeholder
    // Thus, adding a blank parent to each name
    if (parsed) {
      ruleArray = ruleArray.map(({name, value, dontMatch}) => {
        return {name: "." + name, value, dontMatch};
      });
    }
    // Separate out the rules that require expanding properties throughout the
    // view.
    let expandRules = [];
    let rules = ruleArray.filter(rule => {
      if (typeof rule.name == "string" && rule.name.indexOf(".") > -1) {
        expandRules.push(rule);
        return false;
      }
      return true;
    });

    // Search through the view those rules that do not require any properties to
    // be expanded. Build the array of matchers, outstanding promises to be
    // resolved.
    let outstanding = [];

    finder(rules, gUI.view, outstanding);

    // Process the rules that need to expand properties.
    let lastStep = processExpandRules.bind(null, expandRules);

    // Return the results - a promise resolved to hold the updated ruleArray.
    let returnResults = onAllRulesMatched.bind(null, ruleArray);

    return promise.all(outstanding).then(lastStep).then(returnResults);
  }

  function onMatch(prop, rule, matched) {
    if (matched && !rule.matchedProp) {
      rule.matchedProp = prop;
    }
  }

  function finder(rules, view, promises) {
    for (let scope of view) {
      for (let [, prop] of scope) {
        for (let rule of rules) {
          let matcher = matchVariablesViewProperty(prop, rule);
          promises.push(matcher.then(onMatch.bind(null, prop, rule)));
        }
      }
    }
  }

  function processExpandRules(rules) {
    let rule = rules.shift();
    if (!rule) {
      return promise.resolve(null);
    }

    let deferred = promise.defer();
    let expandOptions = {
      rootVariable: gUI.view.getScopeAtIndex(parsed ? 1 : 0),
      expandTo: rule.name
    };

    variablesViewExpandTo(expandOptions).then(function onSuccess(prop) {
      let name = rule.name;
      let lastName = name.split(".").pop();
      rule.name = lastName;

      let matched = matchVariablesViewProperty(prop, rule);
      return matched.then(onMatch.bind(null, prop, rule)).then(function() {
        rule.name = name;
      });
    }, function onFailure() {
      return promise.resolve(null);
    }).then(processExpandRules.bind(null, rules)).then(function() {
      deferred.resolve(null);
    });

    return deferred.promise;
  }

  function onAllRulesMatched(rules) {
    for (let rule of rules) {
      let matched = rule.matchedProp;
      if (matched && !rule.dontMatch) {
        ok(true, "rule " + rule.name + " matched for property " + matched.name);
      } else if (matched && rule.dontMatch) {
        ok(false, "rule " + rule.name + " should not match property " +
           matched.name);
      } else {
        ok(rule.dontMatch, "rule " + rule.name + " did not match any property");
      }
    }
    return rules;
  }

  return init();
}

/**
 * Check if a given Property object from the variables view matches the given
 * rule.
 *
 * @param object prop
 *        The variable's view Property instance.
 * @param object rule
 *        Rules for matching the property. See findVariableViewProperties() for
 *        details.
 * @return object
 *         A promise that is resolved when all the checks complete. Resolution
 *         result is a boolean that tells your promise callback the match
 *         result: true or false.
 */
function matchVariablesViewProperty(prop, rule) {
  function resolve(result) {
    return promise.resolve(result);
  }

  if (!prop) {
    return resolve(false);
  }

  if (rule.name) {
    let match = rule.name instanceof RegExp ?
                rule.name.test(prop.name) :
                prop.name == rule.name;
    if (!match) {
      return resolve(false);
    }
  }

  if ("value" in rule) {
    let displayValue = prop.displayValue;
    if (prop.displayValueClassName == "token-string") {
      displayValue = displayValue.substring(1, displayValue.length - 1);
    }

    let match = rule.value instanceof RegExp ?
                rule.value.test(displayValue) :
                displayValue == rule.value;
    if (!match) {
      info("rule " + rule.name + " did not match value, expected '" +
           rule.value + "', found '" + displayValue + "'");
      return resolve(false);
    }
  }

  return resolve(true);
}

/**
 * Click selects a row in the table.
 *
 * @param {[String]} ids
 *        The array id of the item in the tree
 */
function* selectTreeItem(ids) {
  // Expand tree as some/all items could be collapsed leading to click on an
  // incorrect tree item
  gUI.tree.expandAll();

  let selector = "[data-id='" + JSON.stringify(ids) + "'] > .tree-widget-item";
  let target = gPanelWindow.document.querySelector(selector);
  ok(target, "tree item found with ids " + JSON.stringify(ids));

  let updated = gUI.once("store-objects-updated");

  yield click(target);
  yield updated;
}

/**
 * Click selects a row in the table.
 *
 * @param {String} id
 *        The id of the row in the table widget
 */
function* selectTableItem(id) {
  let selector = ".table-widget-cell[data-id='" + id + "']";
  let target = gPanelWindow.document.querySelector(selector);
  ok(target, "table item found with ids " + id);

  yield click(target);
  yield gUI.once("sidebar-updated");
}

/**
 * Wait for eventName on target.
 * @param {Object} target An observable object that either supports on/off or
 * addEventListener/removeEventListener
 * @param {String} eventName
 * @param {Boolean} [useCapture] for addEventListener/removeEventListener
 * @return A promise that resolves when the event has been handled
 */
function once(target, eventName, useCapture = false) {
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
