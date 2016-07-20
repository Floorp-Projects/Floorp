/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
// shared-head.js handles imports, constants, and utility functions
let sharedHeadURI = testDir + "../../../framework/test/shared-head.js";
Services.scriptloader.loadSubScript(sharedHeadURI, this);

// Import the GCLI test helper
let gcliHelpersURI = testDir + "../../../commandline/test/helpers.js";
Services.scriptloader.loadSubScript(gcliHelpersURI, this);

DevToolsUtils.testing = true;
registerCleanupFunction(() => {
  DevToolsUtils.testing = false;
  Services.prefs.clearUserPref("devtools.responsiveUI.currentPreset");
  Services.prefs.clearUserPref("devtools.responsiveUI.customHeight");
  Services.prefs.clearUserPref("devtools.responsiveUI.customWidth");
  Services.prefs.clearUserPref("devtools.responsiveUI.presets");
  Services.prefs.clearUserPref("devtools.responsiveUI.rotate");
});

SimpleTest.requestCompleteLog();

const { ResponsiveUIManager } = Cu.import("resource://devtools/client/responsivedesign/responsivedesign.jsm", {});

/**
 * Open the Responsive Design Mode
 * @param {Tab} The browser tab to open it into (defaults to the selected tab).
 * @param {method} The method to use to open the RDM (values: menu, keyboard)
 * @return {rdm, manager} Returns the RUI instance and the manager
 */
var openRDM = Task.async(function* (tab = gBrowser.selectedTab,
                                   method = "menu") {
  let manager = ResponsiveUIManager;

  let opened = once(manager, "on");
  let resized = once(manager, "contentResize");
  if (method == "menu") {
    document.getElementById("menu_responsiveUI").doCommand();
  } else {
    synthesizeKeyFromKeyTag(document.getElementById("key_responsiveUI"));
  }
  yield opened;

  let rdm = manager.getResponsiveUIForTab(tab);
  rdm.transitionsEnabled = false;
  registerCleanupFunction(() => {
    rdm.transitionsEnabled = true;
  });

  // Wait for content to resize.  This is triggered async by the preset menu
  // auto-selecting its default entry once it's in the document.
  yield resized;

  return {rdm, manager};
});

/**
 * Close a responsive mode instance
 * @param {rdm} ResponsiveUI instance for the tab
 */
var closeRDM = Task.async(function* (rdm) {
  let manager = ResponsiveUIManager;
  if (!rdm) {
    rdm = manager.getResponsiveUIForTab(gBrowser.selectedTab);
  }
  let closed = once(manager, "off");
  let resized = once(manager, "contentResize");
  rdm.close();
  yield resized;
  yield closed;
});

/**
 * Open the toolbox, with the inspector tool visible.
 * @return a promise that resolves when the inspector is ready
 */
var openInspector = Task.async(function* () {
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
  if (inspector._updateProgress) {
    yield inspector.once("inspector-updated");
  }

  return {
    toolbox: toolbox,
    inspector: inspector
  };
});

var closeToolbox = Task.async(function* () {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  yield gDevTools.closeToolbox(target);
});

/**
 * Wait for the toolbox frame to receive focus after it loads
 * @param {Toolbox} toolbox
 * @return a promise that resolves when focus has been received
 */
function waitForToolboxFrameFocus(toolbox) {
  info("Making sure that the toolbox's frame is focused");
  let def = promise.defer();
  waitForFocus(def.resolve, toolbox.win);
  return def.promise;
}

/**
 * Open the toolbox, with the inspector tool visible, and the sidebar that
 * corresponds to the given id selected
 * @return a promise that resolves when the inspector is ready and the sidebar
 * view is visible and ready
 */
var openInspectorSideBar = Task.async(function* (id) {
  let {toolbox, inspector} = yield openInspector();

  info("Selecting the " + id + " sidebar");
  inspector.sidebar.select(id);

  return {
    toolbox: toolbox,
    inspector: inspector,
    view: inspector[id].view || inspector[id].computedView
  };
});

/**
 * Checks whether the inspector's sidebar corresponding to the given id already
 * exists
 * @param {InspectorPanel}
 * @param {String}
 * @return {Boolean}
 */
function hasSideBarTab(inspector, id) {
  return !!inspector.sidebar.getWindowForTab(id);
}

/**
 * Open the toolbox, with the inspector tool visible, and the computed-view
 * sidebar tab selected.
 * @return a promise that resolves when the inspector is ready and the computed
 * view is visible and ready
 */
function openComputedView() {
  return openInspectorSideBar("computedview");
}

/**
 * Open the toolbox, with the inspector tool visible, and the rule-view
 * sidebar tab selected.
 * @return a promise that resolves when the inspector is ready and the rule
 * view is visible and ready
 */
function openRuleView() {
  return openInspectorSideBar("ruleview");
}

/**
 * Add a new test tab in the browser and load the given url.
 * @param {String} url The url to be loaded in the new tab
 * @return a promise that resolves to the tab object when the url is loaded
 */
var addTab = Task.async(function* (url) {
  info("Adding a new tab with URL: '" + url + "'");

  window.focus();

  let tab = gBrowser.selectedTab = gBrowser.addTab(url);
  let browser = tab.linkedBrowser;

  yield once(browser, "load", true);
  info("URL '" + url + "' loading complete");

  return tab;
});

function wait(ms) {
  let def = promise.defer();
  setTimeout(def.resolve, ms);
  return def.promise;
}

/**
 * Waits for the next load to complete in the current browser.
 *
 * @return promise
 */
function waitForDocLoadComplete(aBrowser = gBrowser) {
  let deferred = promise.defer();
  let progressListener = {
    onStateChange: function (webProgress, req, flags, status) {
      let docStop = Ci.nsIWebProgressListener.STATE_IS_NETWORK |
                    Ci.nsIWebProgressListener.STATE_STOP;
      info(`Saw state ${flags.toString(16)} and status ${status.toString(16)}`);

      // When a load needs to be retargetted to a new process it is cancelled
      // with NS_BINDING_ABORTED so ignore that case
      if ((flags & docStop) == docStop && status != Cr.NS_BINDING_ABORTED) {
        aBrowser.removeProgressListener(progressListener);
        info("Browser loaded");
        deferred.resolve();
      }
    },
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                           Ci.nsISupportsWeakReference])
  };
  aBrowser.addProgressListener(progressListener);
  info("Waiting for browser load");
  return deferred.promise;
}

/**
 * Get the NodeFront for a node that matches a given css selector, via the
 * protocol.
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @return {Promise} Resolves to the NodeFront instance
 */
function getNodeFront(selector, {walker}) {
  if (selector._form) {
    return selector;
  }
  return walker.querySelector(walker.rootNode, selector);
}

/**
 * Set the inspector's current selection to the first match of the given css
 * selector
 * @param {String|NodeFront} selector
 * @param {InspectorPanel} inspector The instance of InspectorPanel currently
 * loaded in the toolbox
 * @param {String} reason Defaults to "test" which instructs the inspector not
 * to highlight the node upon selection
 * @return {Promise} Resolves when the inspector is updated with the new node
 */
var selectNode = Task.async(function* (selector, inspector, reason = "test") {
  info("Selecting the node for '" + selector + "'");
  let nodeFront = yield getNodeFront(selector, inspector);
  let updated = inspector.once("inspector-updated");
  inspector.selection.setNodeFront(nodeFront, reason);
  yield updated;
});

function waitForResizeTo(manager, width, height) {
  return new Promise(resolve => {
    let onResize = (_, data) => {
      if (data.width != width || data.height != height) {
        return;
      }
      manager.off("contentResize", onResize);
      info(`Got contentResize to ${width} x ${height}`);
      resolve();
    };
    info(`Waiting for contentResize to ${width} x ${height}`);
    manager.on("contentResize", onResize);
  });
}

var setPresetIndex = Task.async(function* (rdm, manager, index) {
  info(`Current preset: ${rdm.menulist.selectedIndex}, change to: ${index}`);
  if (rdm.menulist.selectedIndex != index) {
    let resized = once(manager, "contentResize");
    rdm.menulist.selectedIndex = index;
    yield resized;
  }
});

var setSize = Task.async(function* (rdm, manager, width, height) {
  let size = rdm.getSize();
  info(`Current size: ${size.width} x ${size.height}, ` +
       `set to: ${width} x ${height}`);
  if (size.width != width || size.height != height) {
    let resized = waitForResizeTo(manager, width, height);
    rdm.setSize(width, height);
    yield resized;
  }
});
