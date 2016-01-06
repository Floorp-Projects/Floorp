/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let Cu = Components.utils;
let {gDevTools} = Cu.import("resource://devtools/client/framework/gDevTools.jsm", {});
let {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
let {TargetFactory} = require("devtools/client/framework/target");
let {CssComputedView} =
    require("devtools/client/inspector/computed/computed");
let DevToolsUtils = require("devtools/shared/DevToolsUtils");
let promise = require("promise");
let {console} =
  Components.utils.import("resource://gre/modules/Console.jsm", {});

// All tests are asynchronous
waitForExplicitFinish();

const TEST_URL_ROOT =
  "http://example.com/browser/devtools/client/inspector/computed/test/";
const TEST_URL_ROOT_SSL =
  "https://example.com/browser/devtools/client/inspector/computed/test/";
const ROOT_TEST_DIR = getRootDirectory(gTestPath);
const FRAME_SCRIPT_URL = ROOT_TEST_DIR + "doc_frame_script.js";

// Auto clean-up when a test ends
registerCleanupFunction(function*() {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  yield gDevTools.closeToolbox(target);

  while (gBrowser.tabs.length > 1) {
    gBrowser.removeCurrentTab();
  }
});

// Uncomment this pref to dump all devtools emitted events to the console.
// Services.prefs.setBoolPref("devtools.dump.emit", true);

// Set the testing flag on gDevTools and reset it when the test ends
DevToolsUtils.testing = true;
registerCleanupFunction(() => DevToolsUtils.testing = false);

// Clean-up all prefs that might have been changed during a test run
// (safer here because if the test fails, then the pref is never reverted)
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("devtools.inspector.activeSidebar");
  Services.prefs.clearUserPref("devtools.dump.emit");
  Services.prefs.clearUserPref("devtools.defaultColorUnit");
});

/**
 * The functions found below are here to ease test development and maintenance.
 * Most of these functions are stateless and will require some form of context
 * (the instance of the current toolbox, or inspector panel for instance).
 *
 * Most of these functions are async too and return promises.
 *
 * All tests should follow the following pattern:
 *
 * add_task(function*() {
 *   yield addTab(TEST_URI);
 *   let {toolbox, inspector, view} = yield openComputedView();
 *
 *   yield selectNode("#test", inspector);
 *   yield someAsyncTestFunction(view);
 * });
 *
 * add_task is the way to define the testcase in the test file. It accepts
 * a single generator-function argument.
 * The generator function should yield any async call.
 *
 * There is no need to clean tabs up at the end of a test as this is done
 * automatically.
 *
 * It is advised not to store any references on the global scope. There
 * shouldn't be a need to anyway. Thanks to add_task, test steps, even
 * though asynchronous, can be described in a nice flat way, and
 * if/for/while/... control flow can be used as in sync code, making it
 * possible to write the outline of the test case all in add_task, and delegate
 * actual processing and assertions to other functions.
 */

/* *********************************************
 * UTILS
 * *********************************************
 * General test utilities.
 * Add new tabs, open the toolbox and switch to the various panels, select
 * nodes, get node references, ...
 */

/**
 * Add a new test tab in the browser and load the given url.
 *
 * @param {String} url
 *        The url to be loaded in the new tab
 * @return a promise that resolves to the tab object when the url is loaded
 */
function addTab(url) {
  info("Adding a new tab with URL: '" + url + "'");
  let def = promise.defer();

  window.focus();

  let tab = window.gBrowser.selectedTab = window.gBrowser.addTab(url);
  let browser = tab.linkedBrowser;

  info("Loading the helper frame script " + FRAME_SCRIPT_URL);
  browser.messageManager.loadFrameScript(FRAME_SCRIPT_URL, false);

  browser.addEventListener("load", function onload() {
    browser.removeEventListener("load", onload, true);
    info("URL '" + url + "' loading complete");

    def.resolve(tab);
  }, true);

  return def.promise;
}

/**
 * Simple DOM node accesor function that takes either a node or a string css
 * selector as argument and returns the corresponding node
 *
 * @param {String|DOMNode} nodeOrSelector
 * @return {DOMNode|CPOW} Note that in e10s mode a CPOW object is returned which
 * doesn't implement *all* of the DOMNode's properties
 */
function getNode(nodeOrSelector) {
  info("Getting the node for '" + nodeOrSelector + "'");
  return typeof nodeOrSelector === "string" ?
    content.document.querySelector(nodeOrSelector) :
    nodeOrSelector;
}

/**
 * Get the NodeFront for a given css selector, via the protocol
 *
 * @param {String} selector
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox
 * @return {Promise} Resolves to the NodeFront instance
 */
function getNodeFront(selector, {walker}) {
  return walker.querySelector(walker.rootNode, selector);
}

/*
 * Set the inspector's current selection to a node or to the first match of the
 * given css selector.
 *
 * @param {String|NodeFront} data
 *        The node to select
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox
 * @param {String} reason
 *        Defaults to "test" which instructs the inspector not
 *        to highlight the node upon selection
 * @return {Promise} Resolves when the inspector is updated with the new node
 */
var selectNode = Task.async(function*(data, inspector, reason="test") {
  info("Selecting the node for '" + data + "'");
  let nodeFront = data;
  if (!data._form) {
    nodeFront = yield getNodeFront(data, inspector);
  }
  let updated = inspector.once("inspector-updated");
  inspector.selection.setNodeFront(nodeFront, reason);
  yield updated;
});

/**
 * Open the toolbox, with the inspector tool visible.
 *
 * @return a promise that resolves when the inspector is ready
 */
var openInspector = Task.async(function*() {
  info("Opening the inspector");
  let target = TargetFactory.forTab(gBrowser.selectedTab);

  let inspector, toolbox;

  // Checking if the toolbox and the inspector are already loaded
  // The inspector-updated event should only be waited for if the inspector
  // isn't loaded yet
  toolbox = gDevTools.getToolbox(target);
  if (toolbox) {
    inspector = toolbox.getPanel("inspector");
    if (inspector) {
      info("Toolbox and inspector already open");
      return {
        toolbox: toolbox,
        inspector: inspector
      };
    }
  }

  info("Opening the toolbox");
  toolbox = yield gDevTools.showToolbox(target, "inspector");
  yield waitForToolboxFrameFocus(toolbox);
  inspector = toolbox.getPanel("inspector");

  info("Waiting for the inspector to update");
  yield inspector.once("inspector-updated");

  return {
    toolbox: toolbox,
    inspector: inspector
  };
});

/**
 * Wait for the toolbox frame to receive focus after it loads
 *
 * @param {Toolbox} toolbox
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
 * Open the toolbox, with the inspector tool visible, and the sidebar that
 * corresponds to the given id selected
 *
 * @return a promise that resolves when the inspector is ready and the sidebar
 * view is visible and ready
 */
var openInspectorSideBar = Task.async(function*(id) {
  let {toolbox, inspector} = yield openInspector();

  if (!hasSideBarTab(inspector, id)) {
    info("Waiting for the " + id + " sidebar to be ready");
    yield inspector.sidebar.once(id + "-ready");
  }

  info("Selecting the " + id + " sidebar");
  inspector.sidebar.select(id);

  return {
    toolbox: toolbox,
    inspector: inspector,
    view: inspector.sidebar.getWindowForTab(id)[id].view
  };
});

/**
 * Open the toolbox, with the inspector tool visible, and the computed-view
 * sidebar tab selected.
 *
 * @return a promise that resolves when the inspector is ready and the computed
 * view is visible and ready
 */
function openComputedView() {
  return openInspectorSideBar("computedview");
}

/**
 * Wait for eventName on target to be delivered a number of times.
 *
 * @param {Object} target
 *        An observable object that either supports on/off or
 *        addEventListener/removeEventListener
 * @param {String} eventName
 * @param {Number} numTimes
 *        Number of deliveries to wait for.
 * @param {Boolean} useCapture
 *        Optional, for addEventListener/removeEventListener
 * @return A promise that resolves when the event has been handled
 */
function waitForNEvents(target, eventName, numTimes, useCapture = false) {
  info("Waiting for event: '" + eventName + "' on " + target + ".");

  let deferred = promise.defer();
  let count = 0;

  for (let [add, remove] of [
    ["addEventListener", "removeEventListener"],
    ["addListener", "removeListener"],
    ["on", "off"]
  ]) {
    if ((add in target) && (remove in target)) {
      target[add](eventName, function onEvent(...aArgs) {
        if (++count == numTimes) {
          target[remove](eventName, onEvent, useCapture);
          deferred.resolve.apply(deferred, aArgs);
        }
      }, useCapture);
      break;
    }
  }

  return deferred.promise;
}

/**
 * Wait for eventName on target.
 *
 * @param {Object} target
 *        An observable object that either supports on/off or
 *        addEventListener/removeEventListener
 * @param {String} eventName
 * @param {Boolean} useCapture
 *        Optional, for addEventListener/removeEventListener
 * @return A promise that resolves when the event has been handled
 */
function once(target, eventName, useCapture=false) {
  return waitForNEvents(target, eventName, 1, useCapture);
}

/**
 * This shouldn't be used in the tests, but is useful when writing new tests or
 * debugging existing tests in order to introduce delays in the test steps
 *
 * @param {Number} ms
 *        The time to wait
 * @return A promise that resolves when the time is passed
 */
function wait(ms) {
  let def = promise.defer();
  content.setTimeout(def.resolve, ms);
  return def.promise;
}

/**
 * Listen for a new tab to open and return a promise that resolves when one
 * does and completes the load event.
 *
 * @return a promise that resolves to the tab object
 */
var waitForTab = Task.async(function*() {
  info("Waiting for a tab to open");
  yield once(gBrowser.tabContainer, "TabOpen");
  let tab = gBrowser.selectedTab;
  let browser = tab.linkedBrowser;
  yield once(browser, "load", true);
  info("The tab load completed");
  return tab;
});

/**
 * @see SimpleTest.waitForClipboard
 *
 * @param {Function} setup
 *        Function to execute before checking for the
 *        clipboard content
 * @param {String|Boolean} expected
 *        An expected string or validator function
 * @return a promise that resolves when the expected string has been found or
 * the validator function has returned true, rejects otherwise.
 */
function waitForClipboard(setup, expected) {
  let def = promise.defer();
  SimpleTest.waitForClipboard(expected, setup, def.resolve, def.reject);
  return def.promise;
}

/**
 * Dispatch the copy event on the given element
 */
function fireCopyEvent(element) {
  let evt = element.ownerDocument.createEvent("Event");
  evt.initEvent("copy", true, true);
  element.dispatchEvent(evt);
}

/**
 * Checks whether the inspector's sidebar corresponding to the given id already
 * exists
 *
 * @param {InspectorPanel}
 * @param {String}
 * @return {Boolean}
 */
function hasSideBarTab(inspector, id) {
  return !!inspector.sidebar.getWindowForTab(id);
}

/**
 * Simulate the key input for the given input in the window.
 *
 * @param {String} input
 *        The string value to input
 * @param {Window} win
 *        The window containing the panel
 */
function synthesizeKeys(input, win) {
  for (let key of input.split("")) {
    EventUtils.synthesizeKey(key, {}, win);
  }
}

/* *********************************************
 * COMPUTED-VIEW
 * *********************************************
 * Computed-view related utility functions.
 * Allows to get properties, links, expand properties, ...
 */

/**
 * Get references to the name and value span nodes corresponding to a given
 * property name in the computed-view
 *
 * @param {CssComputedView} view
 *        The instance of the computed view panel
 * @param {String} name
 *        The name of the property to retrieve
 * @return an object {nameSpan, valueSpan}
 */
function getComputedViewProperty(view, name) {
  let prop;
  for (let property of view.styleDocument.querySelectorAll(".property-view")) {
    let nameSpan = property.querySelector(".property-name");
    let valueSpan = property.querySelector(".property-value");

    if (nameSpan.textContent === name) {
      prop = {nameSpan: nameSpan, valueSpan: valueSpan};
      break;
    }
  }
  return prop;
}

/**
 * Get an instance of PropertyView from the computed-view.
 *
 * @param {CssComputedView} view
 *        The instance of the computed view panel
 * @param {String} name
 *        The name of the property to retrieve
 * @return {PropertyView}
 */
function getComputedViewPropertyView(view, name) {
  let propView;
  for (let propertyView of view.propertyViews) {
    if (propertyView._propertyInfo.name === name) {
      propView = propertyView;
      break;
    }
  }
  return propView;
}

/**
 * Get a reference to the property-content element for a given property name in
 * the computed-view.
 * A property-content element always follows (nextSibling) the property itself
 * and is only shown when the twisty icon is expanded on the property.
 * A property-content element contains matched rules, with selectors,
 * properties, values and stylesheet links
 *
 * @param {CssComputedView} view
 *        The instance of the computed view panel
 * @param {String} name
 *        The name of the property to retrieve
 * @return {Promise} A promise that resolves to the property matched rules
 * container
 */
var getComputedViewMatchedRules = Task.async(function*(view, name) {
  let expander;
  let propertyContent;
  for (let property of view.styleDocument.querySelectorAll(".property-view")) {
    let nameSpan = property.querySelector(".property-name");
    if (nameSpan.textContent === name) {
      expander = property.querySelector(".expandable");
      propertyContent = property.nextSibling;
      break;
    }
  }

  if (!expander.hasAttribute("open")) {
    // Need to expand the property
    let onExpand = view.inspector.once("computed-view-property-expanded");
    expander.click();
    yield onExpand;
  }

  return propertyContent;
});

/**
 * Get the text value of the property corresponding to a given name in the
 * computed-view
 *
 * @param {CssComputedView} view
 *        The instance of the computed view panel
 * @param {String} name
 *        The name of the property to retrieve
 * @return {String} The property value
 */
function getComputedViewPropertyValue(view, name, propertyName) {
  return getComputedViewProperty(view, name, propertyName)
    .valueSpan.textContent;
}

/**
 * Expand a given property, given its index in the current property list of
 * the computed view
 *
 * @param {CssComputedView} view
 *        The instance of the computed view panel
 * @param {Number} index
 *        The index of the property to be expanded
 * @return a promise that resolves when the property has been expanded, or
 * rejects if the property was not found
 */
function expandComputedViewPropertyByIndex(view, index) {
  info("Expanding property " + index + " in the computed view");
  let expandos = view.styleDocument.querySelectorAll(".expandable");
  if (!expandos.length || !expandos[index]) {
    return promise.reject();
  }

  let onExpand = view.inspector.once("computed-view-property-expanded");
  expandos[index].click();
  return onExpand;
}

/**
 * Get a rule-link from the computed-view given its index
 *
 * @param {CssComputedView} view
 *        The instance of the computed view panel
 * @param {Number} index
 *        The index of the link to be retrieved
 * @return {DOMNode} The link at the given index, if one exists, null otherwise
 */
function getComputedViewLinkByIndex(view, index) {
  let links = view.styleDocument.querySelectorAll(".rule-link .link");
  return links[index];
}

/* *********************************************
 * STYLE-EDITOR
 * *********************************************
 * Style-editor related utility functions.
 */

/**
 * Wait for the toolbox to emit the styleeditor-selected event and when done
 * wait for the stylesheet identified by href to be loaded in the stylesheet
 * editor
 *
 * @param {Toolbox} toolbox
 * @param {String} href
 *        Optional, if not provided, wait for the first editor to be ready
 * @return a promise that resolves to the editor when the stylesheet editor is
 * ready
 */
function waitForStyleEditor(toolbox, href) {
  let def = promise.defer();

  info("Waiting for the toolbox to switch to the styleeditor");
  toolbox.once("styleeditor-selected").then(() => {
    let panel = toolbox.getCurrentPanel();
    ok(panel && panel.UI, "Styleeditor panel switched to front");

    // A helper that resolves the promise once it receives an editor that
    // matches the expected href. Returns false if the editor was not correct.
    let gotEditor = (event, editor) => {
      let currentHref = editor.styleSheet.href;
      if (!href || (href && currentHref.endsWith(href))) {
        info("Stylesheet editor selected");
        panel.UI.off("editor-selected", gotEditor);

        editor.getSourceEditor().then(sourceEditor => {
          info("Stylesheet editor fully loaded");
          def.resolve(sourceEditor);
        });

        return true;
      }

      info("The editor was incorrect. Waiting for editor-selected event.");
      return false;
    };

    // The expected editor may already be selected. Check the if the currently
    // selected editor is the expected one and if not wait for an
    // editor-selected event.
    if (!gotEditor("styleeditor-selected", panel.UI.selectedEditor)) {
      // The expected editor is not selected (yet). Wait for it.
      panel.UI.on("editor-selected", gotEditor);
    }
  });

  return def.promise;
}

/**
 * Reload the current page and wait for the inspector to be initialized after
 * the navigation
 *
 * @param {InspectorPanel} inspector
 *        The instance of InspectorPanel currently loaded in the toolbox
 * @return a promise that resolves after page reload and inspector
 * initialization
 */
function reloadPage(inspector) {
  let onNewRoot = inspector.once("new-root");
  content.location.reload();
  return onNewRoot.then(() => {
    inspector.markup._waitForChildren();
  });
}
