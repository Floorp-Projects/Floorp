 /* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// shared-head.js handles imports, constants, and utility functions
Services.scriptloader.loadSubScript("chrome://mochitests/content/browser/devtools/client/framework/test/shared-head.js", this);

const BASE_URI = "http://mochi.test:8888/browser/devtools/client/fontinspector/test/"

/**
 * Open the toolbox, with the inspector tool visible.
 * @param {Function} cb Optional callback, if you don't want to use the returned
 * promise
 * @return a promise that resolves when the inspector is ready
 */
var openInspector = Task.async(function*(cb) {
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
      if (cb) {
        return cb(inspector, toolbox);
      } else {
        return {
          toolbox: toolbox,
          inspector: inspector
        };
      }
    }
  }

  info("Opening the toolbox");
  toolbox = yield gDevTools.showToolbox(target, "inspector");
  yield waitForToolboxFrameFocus(toolbox);
  inspector = toolbox.getPanel("inspector");

  info("Waiting for the inspector to update");
  yield inspector.once("inspector-updated");

  if (cb) {
    return cb(inspector, toolbox);
  } else {
    return {
      toolbox: toolbox,
      inspector: inspector
    };
  }
});

/**
 * Adds a new tab with the given URL, opens the inspector and selects the
 * font-inspector tab.
 *
 * @return Object
 *  {
 *    toolbox,
 *    inspector,
 *    fontInspector
 *  }
 */
var openFontInspectorForURL = Task.async(function* (url) {
  info("Opening tab " + url);
  yield addTab(url);

  let { toolbox, inspector } = yield openInspector();

  /**
   * Call selectNode to trigger font-inspector update so that we don't timeout
   * if following conditions hold
   * a) the initial 'fontinspector-updated' was emitted while we were waiting
   *    for openInspector to resolve
   * b) the font-inspector tab was selected by default which means the call to
   *    select will not trigger another update.
   *
   * selectNode calls setNodeFront which always emits 'new-node' which calls
   * FontInspector.update that emits the 'fontinspector-updated' event.
   */
  let onUpdated = inspector.once("fontinspector-updated");

  yield selectNode("body", inspector);
  inspector.sidebar.select("fontinspector");

  info("Waiting for font-inspector to update.");
  yield onUpdated;

  info("Font Inspector ready.");

  let { fontInspector } = inspector.sidebar.getWindowForTab("fontinspector");
  return {
    fontInspector,
    inspector,
    toolbox
  };
});

/**
 * Select a node in the inspector given its selector.
 */
var selectNode = Task.async(function*(selector, inspector, reason="test") {
  info("Selecting the node for '" + selector + "'");
  let nodeFront = yield getNodeFront(selector, inspector);
  let updated = inspector.once("inspector-updated");
  inspector.selection.setNodeFront(nodeFront, reason);
  yield updated;
});

/**
 * Get the NodeFront for a given css selector, via the protocol
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
 * Wait for the toolbox frame to receive focus after it loads
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
 * Clears the preview input field, types new text into it and waits for the
 * preview images to be updated.
 *
 * @param {FontInspector} fontInspector - The FontInspector instance.
 * @param {String} text - The text to preview.
 */
function* updatePreviewText(fontInspector, text) {
  info(`Changing the preview text to '${text}'`);

  let doc = fontInspector.chromeDoc;
  let input = doc.getElementById("preview-text-input");
  let update = fontInspector.inspector.once("fontinspector-updated");

  info("Focusing the input field.");
  input.focus();

  is(doc.activeElement, input, "The input was focused.");

  info("Blanking the input field.");
  for (let i = input.value.length; i >= 0; i--) {
    EventUtils.sendKey("BACK_SPACE", doc.defaultView);
  }

  is(input.value, "", "The input is now blank.");

  info("Typing the specified text to the input field.");
  EventUtils.sendString(text, doc.defaultView);
  is(input.value, text, "The input now contains the correct text.");

  info("Waiting for the font-inspector to update.");
  yield update;
}
