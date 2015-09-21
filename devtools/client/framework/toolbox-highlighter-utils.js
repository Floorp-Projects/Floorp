/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");
const promise = require("promise");
Cu.import("resource://gre/modules/Task.jsm");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

/**
 * Client-side highlighter shared module.
 * To be used by toolbox panels that need to highlight DOM elements.
 *
 * Highlighting and selecting elements is common enough that it needs to be at
 * toolbox level, accessible by any panel that needs it.
 * That's why the toolbox is the one that initializes the inspector and
 * highlighter. It's also why the API returned by this module needs a reference
 * to the toolbox which should be set once only.
 */

/**
 * Get the highighterUtils instance for a given toolbox.
 * This should be done once only by the toolbox itself and stored there so that
 * panels can get it from there. That's because the API returned has a stateful
 * scope that would be different for another instance returned by this function.
 *
 * @param {Toolbox} toolbox
 * @return {Object} the highlighterUtils public API
 */
exports.getHighlighterUtils = function(toolbox) {
  if (!toolbox || !toolbox.target) {
    throw new Error("Missing or invalid toolbox passed to getHighlighterUtils");
    return;
  }

  // Exported API properties will go here
  let exported = {};

  // The current toolbox target
  let target = toolbox.target;

  // Is the highlighter currently in pick mode
  let isPicking = false;

  // Is the box model already displayed, used to prevent dispatching
  // unnecessary requests, especially during toolbox shutdown
  let isNodeFrontHighlighted = false;

  /**
   * Release this utils, nullifying the references to the toolbox
   */
  exported.release = function() {
    toolbox = target = null;
  }

  /**
   * Does the target have the highlighter actor.
   * The devtools must be backwards compatible with at least B2G 1.3 (28),
   * which doesn't have the highlighter actor. This can be removed as soon as
   * the minimal supported version becomes 1.4 (29)
   */
  let isRemoteHighlightable = exported.isRemoteHighlightable = function() {
    return target.client.traits.highlightable;
  }

  /**
   * Does the target support custom highlighters.
   */
  let supportsCustomHighlighters = exported.supportsCustomHighlighters = () => {
    return !!target.client.traits.customHighlighters;
  };

  /**
   * Make a function that initializes the inspector before it runs.
   * Since the init of the inspector is asynchronous, the return value will be
   * produced by Task.async and the argument should be a generator
   * @param {Function*} generator A generator function
   * @return {Function} A function
   */
  let isInspectorInitialized = false;
  let requireInspector = generator => {
    return Task.async(function*(...args) {
      if (!isInspectorInitialized) {
        yield toolbox.initInspector();
        isInspectorInitialized = true;
      }
      return yield generator.apply(null, args);
    });
  };

  /**
   * Start/stop the element picker on the debuggee target.
   * @return A promise that resolves when done
   */
  let togglePicker = exported.togglePicker = function() {
    if (isPicking) {
      return stopPicker();
    } else {
      return startPicker();
    }
  }

  /**
   * Start the element picker on the debuggee target.
   * This will request the inspector actor to start listening for mouse events
   * on the target page to highlight the hovered/picked element.
   * Depending on the server-side capabilities, this may fire events when nodes
   * are hovered.
   * @return A promise that resolves when the picker has started or immediately
   * if it is already started
   */
  let startPicker = exported.startPicker = requireInspector(function*() {
    if (isPicking) {
      return;
    }
    isPicking = true;

    toolbox.pickerButtonChecked = true;
    yield toolbox.selectTool("inspector");
    toolbox.on("select", stopPicker);

    if (isRemoteHighlightable()) {
      toolbox.walker.on("picker-node-hovered", onPickerNodeHovered);
      toolbox.walker.on("picker-node-picked", onPickerNodePicked);
      toolbox.walker.on("picker-node-canceled", onPickerNodeCanceled);

      yield toolbox.highlighter.pick();
      toolbox.emit("picker-started");
    } else {
      // If the target doesn't have the highlighter actor, we can use the
      // walker's pick method instead, knowing that it only responds when a node
      // is picked (instead of emitting events)
      toolbox.emit("picker-started");
      let node = yield toolbox.walker.pick();
      onPickerNodePicked({node: node});
    }
  });

  /**
   * Stop the element picker. Note that the picker is automatically stopped when
   * an element is picked
   * @return A promise that resolves when the picker has stopped or immediately
   * if it is already stopped
   */
  let stopPicker = exported.stopPicker = requireInspector(function*() {
    if (!isPicking) {
      return;
    }
    isPicking = false;

    toolbox.pickerButtonChecked = false;

    if (isRemoteHighlightable()) {
      yield toolbox.highlighter.cancelPick();
      toolbox.walker.off("picker-node-hovered", onPickerNodeHovered);
      toolbox.walker.off("picker-node-picked", onPickerNodePicked);
      toolbox.walker.off("picker-node-canceled", onPickerNodeCanceled);
    } else {
      // If the target doesn't have the highlighter actor, use the walker's
      // cancelPick method instead
      yield toolbox.walker.cancelPick();
    }

    toolbox.off("select", stopPicker);
    toolbox.emit("picker-stopped");
  });

  /**
   * When a node is hovered by the mouse when the highlighter is in picker mode
   * @param {Object} data Information about the node being hovered
   */
  function onPickerNodeHovered(data) {
    toolbox.emit("picker-node-hovered", data.node);
  }

  /**
   * When a node has been picked while the highlighter is in picker mode
   * @param {Object} data Information about the picked node
   */
  function onPickerNodePicked(data) {
    toolbox.selection.setNodeFront(data.node, "picker-node-picked");
    stopPicker();
  }

  /**
   * When the picker is canceled, stop the picker, and make sure the toolbox
   * gets the focus.
   */
  function onPickerNodeCanceled() {
    stopPicker();
    toolbox.frame.focus();
  }

  /**
   * Show the box model highlighter on a node in the content page.
   * The node needs to be a NodeFront, as defined by the inspector actor
   * @see toolkit/devtools/server/actors/inspector.js
   * @param {NodeFront} nodeFront The node to highlight
   * @param {Object} options
   * @return A promise that resolves when the node has been highlighted
   */
  let highlightNodeFront = exported.highlightNodeFront = requireInspector(
  function*(nodeFront, options={}) {
    if (!nodeFront) {
      return;
    }

    isNodeFrontHighlighted = true;
    if (isRemoteHighlightable()) {
      yield toolbox.highlighter.showBoxModel(nodeFront, options);
    } else {
      // If the target doesn't have the highlighter actor, revert to the
      // walker's highlight method, which draws a simple outline
      yield toolbox.walker.highlight(nodeFront);
    }

    toolbox.emit("node-highlight", nodeFront, options.toSource());
  });

  /**
   * This is a convenience method in case you don't have a nodeFront but a
   * valueGrip. This is often the case with VariablesView properties.
   * This method will simply translate the grip into a nodeFront and call
   * highlightNodeFront, so it has the same signature.
   * @see highlightNodeFront
   */
  let highlightDomValueGrip = exported.highlightDomValueGrip = requireInspector(
  function*(valueGrip, options={}) {
    let nodeFront = yield gripToNodeFront(valueGrip);
    if (nodeFront) {
      yield highlightNodeFront(nodeFront, options);
    } else {
      throw new Error("The ValueGrip passed could not be translated to a NodeFront");
    }
  });

  /**
   * Translate a debugger value grip into a node front usable by the inspector
   * @param {ValueGrip}
   * @return a promise that resolves to the node front when done
   */
  let gripToNodeFront = exported.gripToNodeFront = requireInspector(
  function*(grip) {
    return yield toolbox.walker.getNodeActorFromObjectActor(grip.actor);
  });

  /**
   * Hide the highlighter.
   * @param {Boolean} forceHide Only really matters in test mode (when
   * DevToolsUtils.testing is true). In test mode, hovering over several nodes
   * in the markup view doesn't hide/show the highlighter to ease testing. The
   * highlighter stays visible at all times, except when the mouse leaves the
   * markup view, which is when this param is passed to true
   * @return a promise that resolves when the highlighter is hidden
   */
  let unhighlight = exported.unhighlight = Task.async(
  function*(forceHide=false) {
    forceHide = forceHide || !DevToolsUtils.testing;

    // Note that if isRemoteHighlightable is true, there's no need to hide the
    // highlighter as the walker uses setTimeout to hide it after some time
    if (isNodeFrontHighlighted && forceHide && toolbox.highlighter && isRemoteHighlightable()) {
      isNodeFrontHighlighted = false;
      yield toolbox.highlighter.hideBoxModel();
    }

    toolbox.emit("node-unhighlight");
  });

  /**
   * If the main, box-model, highlighter isn't enough, or if multiple
   * highlighters are needed in parallel, this method can be used to return a
   * new instance of a highlighter actor, given a type.
   * The type of the highlighter passed must be known by the server.
   * The highlighter actor returned will have the show(nodeFront) and hide()
   * methods and needs to be released by the consumer when not needed anymore.
   * @return a promise that resolves to the highlighter
   */
  let getHighlighterByType = exported.getHighlighterByType = requireInspector(
  function*(typeName) {
    let highlighter = null;

    if (supportsCustomHighlighters()) {
      highlighter = yield toolbox.inspector.getHighlighterByType(typeName);
    }

    return highlighter || promise.reject("The target doesn't support " +
        `creating highlighters by types or ${typeName} is unknown`);

  });

  // Return the public API
  return exported;
};
