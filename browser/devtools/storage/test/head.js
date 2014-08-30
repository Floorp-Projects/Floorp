/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let tempScope = {};
Cu.import("resource://gre/modules/devtools/Loader.jsm", tempScope);
Cu.import("resource://gre/modules/devtools/Console.jsm", tempScope);
const console = tempScope.console;
const devtools = tempScope.devtools;
tempScope = null;
const require = devtools.require;
const TargetFactory = devtools.TargetFactory;

const SPLIT_CONSOLE_PREF = "devtools.toolbox.splitconsoleEnabled";
const STORAGE_PREF = "devtools.storage.enabled";
const PATH = "browser/browser/devtools/storage/test/";
const MAIN_DOMAIN = "http://test1.example.org/" + PATH;
const ALT_DOMAIN = "http://sectest1.example.org/" + PATH;
const ALT_DOMAIN_SECURED = "https://sectest1.example.org:443/" + PATH;

let {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});

waitForExplicitFinish();

let gToolbox, gPanelWindow, gWindow, gUI;

Services.prefs.setBoolPref(STORAGE_PREF, true);
gDevTools.testing = true;
registerCleanupFunction(() => {
  gToolbox = gPanelWindow = gWindow = gUI = null;
  Services.prefs.clearUserPref(STORAGE_PREF);
  Services.prefs.clearUserPref(SPLIT_CONSOLE_PREF);
  gDevTools.testing = false;
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
 * Opens the given url in a new tab, then sets up the page by waiting for
 * all cookies, indexedDB items etc. to be created; Then opens the storage
 * inspector and waits for the storage tree and table to be populated
 *
 * @param url {String} The url to be opened in the new tab
 *
 * @return {Promise} A promise that resolves after storage inspector is ready
 */
let openTabAndSetupStorage = Task.async(function*(url) {
  /**
   * This method iterates over iframes in a window and setups the indexed db
   * required for this test.
   */
  let setupIDBInFrames = (w, i, c) => {
    if (w[i] && w[i].idbGenerator) {
      w[i].setupIDB = w[i].idbGenerator(() => setupIDBInFrames(w, i + 1, c));
      w[i].setupIDB.next();
    }
    else if (w[i] && w[i + 1]) {
      setupIDBInFrames(w, i + 1, c);
    }
    else {
      c();
    }
  };

  let content = yield addTab(url);

  let def = promise.defer();
  // Setup the indexed db in main window.
  gWindow = content.wrappedJSObject;
  if (gWindow.idbGenerator) {
    gWindow.setupIDB = gWindow.idbGenerator(() => {
      setupIDBInFrames(gWindow, 0, () => {
        def.resolve();
      });
    });
    gWindow.setupIDB.next();
    yield def.promise;
  }

  // open storage inspector
  return yield openStoragePanel();
});

/**
 * Open the toolbox, with the storage tool visible.
 *
 * @param cb {Function} Optional callback, if you don't want to use the returned
 *                      promise
 *
 * @return {Promise} a promise that resolves when the storage inspector is ready
 */
let openStoragePanel = Task.async(function*(cb) {
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
      } else {
        return {
          toolbox: toolbox,
          storage: storage
        };
      }
    }
  }

  info("Opening the toolbox");
  toolbox = yield gDevTools.showToolbox(target, "storage");
  storage = toolbox.getPanel("storage");
  gPanelWindow = storage.panelWindow;
  gUI = storage.UI;
  gToolbox = toolbox;

  info("Waiting for the stores to update");
  yield gUI.once("store-objects-updated");

  yield waitForToolboxFrameFocus(toolbox);

  if (cb) {
    return cb(storage, toolbox);
  } else {
    return {
      toolbox: toolbox,
      storage: storage
    };
  }
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
function finishTests() {
  // Cleanup so that indexed db created from this test do not interfere next ones

  /**
   * This method iterates over iframes in a window and clears the indexed db
   * created by this test.
   */
  let clearIDB = (w, i, c) => {
    if (w[i] && w[i].clear) {
      w[i].clearIterator = w[i].clear(() => clearIDB(w, i + 1, c));
      w[i].clearIterator.next();
    }
    else if (w[i] && w[i + 1]) {
      clearIDB(w, i + 1, c);
    }
    else {
      c();
    }
  };

  gWindow.clearIterator = gWindow.clear(() => {
    clearIDB(gWindow, 0, () => {
      // Forcing GC/CC to get rid of docshells and windows created by this test.
      forceCollections();
      finish();
    });
  });
  gWindow.clearIterator.next();
}

// Sends a click event on the passed DOM node in an async manner
function click(node) {
  node.scrollIntoView()
  executeSoon(() => EventUtils.synthesizeMouseAtCenter(node, {}, gPanelWindow));
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
 * @return object
 *         A promise that is resolved only when the last property in |expandTo|
 *         is found, and rejected otherwise. Resolution reason is always the
 *         last property - |nextSibling| in the example above. Rejection is
 *         always the last property that was found.
 */
function variablesViewExpandTo(aOptions) {
  let root = aOptions.rootVariable;
  let expandTo = aOptions.expandTo.split(".");
  let lastDeferred = promise.defer();

  function getNext(aProp) {
    let name = expandTo.shift();
    let newProp = aProp.get(name);

    if (expandTo.length > 0) {
      ok(newProp, "found property " + name);
      if (newProp && newProp.expand) {
        newProp.expand();
        getNext(newProp);
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

  function fetchError(aProp) {
    lastDeferred.reject(aProp);
  }

  if (root && root.expand) {
    root.expand();
    getNext(root);
  }
  else {
    lastDeferred.resolve(root)
  }

  return lastDeferred.promise;
}

/**
 * Find variables or properties in a VariablesView instance.
 *
 * @param array aRules
 *        The array of rules you want to match. Each rule is an object with:
 *        - name (string|regexp): property name to match.
 *        - value (string|regexp): property value to match.
 *        - dontMatch (boolean): make sure the rule doesn't match any property.
 * @param boolean aParsed
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
function findVariableViewProperties(aRules, aParsed) {
  // Initialize the search.
  function init() {
    // If aParsed is true, we are checking rules in the parsed value section of
    // the storage sidebar. That scope uses a blank variable as a placeholder
    // Thus, adding a blank parent to each name
    if (aParsed) {
      aRules = aRules.map(({name, value, dontMatch}) => {
        return {name: "." + name, value, dontMatch}
      });
    }
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

    finder(rules, gUI.view, outstanding);

    // Process the rules that need to expand properties.
    let lastStep = processExpandRules.bind(null, expandRules);

    // Return the results - a promise resolved to hold the updated aRules array.
    let returnResults = onAllRulesMatched.bind(null, aRules);

    return promise.all(outstanding).then(lastStep).then(returnResults);
  }

  function onMatch(aProp, aRule, aMatched) {
    if (aMatched && !aRule.matchedProp) {
      aRule.matchedProp = aProp;
    }
  }

  function finder(aRules, aView, aPromises) {
    for (let scope of aView) {
      for (let [id, prop] of scope) {
        for (let rule of aRules) {
          let matcher = matchVariablesViewProperty(prop, rule);
          aPromises.push(matcher.then(onMatch.bind(null, prop, rule)));
        }
      }
    }
  }

  function processExpandRules(aRules) {
    let rule = aRules.shift();
    if (!rule) {
      return promise.resolve(null);
    }

    let deferred = promise.defer();
    let expandOptions = {
      rootVariable: gUI.view.getScopeAtIndex(aParsed ? 1: 0),
      expandTo: rule.name
    };

    variablesViewExpandTo(expandOptions).then(function onSuccess(aProp) {
      let name = rule.name;
      let lastName = name.split(".").pop();
      rule.name = lastName;

      let matched = matchVariablesViewProperty(aProp, rule);
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

  function onAllRulesMatched(aRules) {
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
 * @return object
 *         A promise that is resolved when all the checks complete. Resolution
 *         result is a boolean that tells your promise callback the match
 *         result: true or false.
 */
function matchVariablesViewProperty(aProp, aRule) {
  function resolve(aResult) {
    return promise.resolve(aResult);
  }

  if (!aProp) {
    return resolve(false);
  }

  if (aRule.name) {
    let match = aRule.name instanceof RegExp ?
                aRule.name.test(aProp.name) :
                aProp.name == aRule.name;
    if (!match) {
      return resolve(false);
    }
  }

  if ("value" in aRule) {
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

  return resolve(true);
}

/**
 * Click selects a row in the table.
 *
 * @param {[String]} ids
 *        The array id of the item in the tree
 */
function selectTreeItem(ids) {
  // Expand tree as some/all items could be collapsed leading to click on an
  // incorrect tree item
  gUI.tree.expandAll();
  click(gPanelWindow.document.querySelector("[data-id='" + JSON.stringify(ids) +
        "'] > .tree-widget-item"));
}

/**
 * Click selects a row in the table.
 *
 * @param {String} id
 *        The id of the row in the table widget
 */
function selectTableItem(id) {
  click(gPanelWindow.document.querySelector(".table-widget-cell[data-id='" +
        id + "']"));
}
