/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const EventEmitter = require("devtools/shared/old-event-emitter");
const {
  VIEW_NODE_VALUE_TYPE,
  VIEW_NODE_SHAPE_POINT_TYPE
} = require("devtools/client/inspector/shared/node-types");

const DEFAULT_GRID_COLOR = "#4B0082";
const INSET_POINT_TYPES = ["top", "right", "bottom", "left"];

/**
 * Highlighters overlay is a singleton managing all highlighters in the Inspector.
 */
class HighlightersOverlay {
  /**
   * @param  {Inspector} inspector
   *         Inspector toolbox panel.
   */
  constructor(inspector) {
    this.inspector = inspector;
    this.highlighters = {};
    this.highlighterUtils = this.inspector.toolbox.highlighterUtils;

    // Only initialize the overlay if at least one of the highlighter types is supported.
    this.supportsHighlighters = this.highlighterUtils.supportsCustomHighlighters();

    // NodeFront of the flexbox container that is highlighted.
    this.flexboxHighlighterShown = null;
    // NodeFront of element that is highlighted by the geometry editor.
    this.geometryEditorHighlighterShown = null;
    // NodeFront of the grid container that is highlighted.
    this.gridHighlighterShown = null;
    // Name of the highlighter shown on mouse hover.
    this.hoveredHighlighterShown = null;
    // Name of the selector highlighter shown.
    this.selectorHighlighterShown = null;
    // NodeFront of the shape that is highlighted
    this.shapesHighlighterShown = null;
    // Saved state to be restore on page navigation.
    this.state = {
      flexbox: {},
      grid: {},
      shapes: {},
    };

    this.onClick = this.onClick.bind(this);
    this.onMarkupMutation = this.onMarkupMutation.bind(this);
    this.onMouseMove = this.onMouseMove.bind(this);
    this.onMouseOut = this.onMouseOut.bind(this);
    this.onWillNavigate = this.onWillNavigate.bind(this);
    this.hideFlexboxHighlighter = this.hideFlexboxHighlighter.bind(this);
    this.hideGridHighlighter = this.hideGridHighlighter.bind(this);
    this.hideShapesHighlighter = this.hideShapesHighlighter.bind(this);
    this.showFlexboxHighlighter = this.showFlexboxHighlighter.bind(this);
    this.showGridHighlighter = this.showGridHighlighter.bind(this);
    this.showShapesHighlighter = this.showShapesHighlighter.bind(this);
    this._handleRejection = this._handleRejection.bind(this);
    this._onHighlighterEvent = this._onHighlighterEvent.bind(this);

    // Add inspector events, not specific to a given view.
    this.inspector.on("markupmutation", this.onMarkupMutation);
    this.inspector.target.on("will-navigate", this.onWillNavigate);

    EventEmitter.decorate(this);
  }

  /**
   * Returns whether `node` is somewhere inside the DOM of the rule view.
   *
   * @param {DOMNode} node
   * @return {Boolean}
   */
  isRuleView(node) {
    return !!node.closest("#ruleview-panel");
  }

  /**
   * Add the highlighters overlay to the view. This will start tracking mouse events
   * and display highlighters when needed.
   *
   * @param  {CssRuleView|CssComputedView|LayoutView} view
   *         Either the rule-view or computed-view panel to add the highlighters overlay.
   */
  addToView(view) {
    if (!this.supportsHighlighters) {
      return;
    }

    let el = view.element;
    el.addEventListener("click", this.onClick, true);
    el.addEventListener("mousemove", this.onMouseMove);
    el.addEventListener("mouseout", this.onMouseOut);
    el.ownerDocument.defaultView.addEventListener("mouseout", this.onMouseOut);
  }

  /**
   * Remove the overlay from the given view. This will stop tracking mouse movement and
   * showing highlighters.
   *
   * @param  {CssRuleView|CssComputedView|LayoutView} view
   *         Either the rule-view or computed-view panel to remove the highlighters
   *         overlay.
   */
  removeFromView(view) {
    if (!this.supportsHighlighters) {
      return;
    }

    let el = view.element;
    el.removeEventListener("click", this.onClick, true);
    el.removeEventListener("mousemove", this.onMouseMove);
    el.removeEventListener("mouseout", this.onMouseOut);
  }

  /**
   * Toggle the shapes highlighter for the given element with a shape.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the element with a shape to highlight.
   * @param  {Object} options
   *         Object used for passing options to the shapes highlighter.
   */
  async toggleShapesHighlighter(node, options = {}) {
    options.transformMode = options.ctrlOrMetaPressed;

    if (node == this.shapesHighlighterShown &&
        options.mode === this.state.shapes.options.mode) {
      // If meta/ctrl is not pressed, hide the highlighter.
      if (!options.ctrlOrMetaPressed) {
        await this.hideShapesHighlighter(node);
        return;
      }

      // If meta/ctrl is pressed, toggle transform mode on the highlighter.
      options.transformMode = !this.state.shapes.options.transformMode;
    }

    await this.showShapesHighlighter(node, options);
  }

  /**
   * Show the shapes highlighter for the given element with a shape.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the element with a shape to highlight.
   * @param  {Object} options
   *         Object used for passing options to the shapes highlighter.
   */
  async showShapesHighlighter(node, options) {
    let highlighter = await this._getHighlighter("ShapesHighlighter");
    if (!highlighter) {
      return;
    }

    let isShown = await highlighter.show(node, options);
    if (!isShown) {
      return;
    }

    this.shapesHighlighterShown = node;
    let { mode } = options;
    this._toggleRuleViewIcon(node, false, ".ruleview-shape");
    this._toggleRuleViewIcon(node, true, `.ruleview-shape[data-mode='${mode}']`);

    try {
      // Save shapes highlighter state.
      let { url } = this.inspector.target;
      let selector = await node.getUniqueSelector();
      this.state.shapes = { selector, options, url };
      this.shapesHighlighterShown = node;
      this.emit("shapes-highlighter-shown", node, options);
    } catch (e) {
      this._handleRejection(e);
    }
  }

  /**
   * Hide the shapes highlighter for the given element with a shape.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the element with a shape to unhighlight.
   */
  async hideShapesHighlighter(node) {
    if (!this.shapesHighlighterShown || !this.highlighters.ShapesHighlighter) {
      return;
    }

    this._toggleRuleViewIcon(node, false, ".ruleview-shape");

    await this.highlighters.ShapesHighlighter.hide();
    this.emit("shapes-highlighter-hidden", this.shapesHighlighterShown,
      this.state.shapes.options);
    this.shapesHighlighterShown = null;
    this.state.shapes = {};
  }

  /**
   * Show the shapes highlighter for the given element, with the given point highlighted.
   *
   * @param {NodeFront} node
   *        The NodeFront of the element to highlight.
   * @param {String} point
   *        The point to highlight in the shapes highlighter.
   */
  async hoverPointShapesHighlighter(node, point) {
    if (node == this.shapesHighlighterShown) {
      let options = Object.assign({}, this.state.shapes.options);
      options.hoverPoint = point;
      await this.showShapesHighlighter(node, options);
    }
  }

  /**
   * Highlight the given shape point in the rule view.
   *
   * @param {String} point
   *        The point to highlight.
   */
  highlightRuleViewShapePoint(point) {
    let view = this.inspector.getPanel("ruleview").view;
    let ruleViewEl = view.element;
    let selector = `.ruleview-shape-point.active`;

    for (let pointNode of ruleViewEl.querySelectorAll(selector)) {
      this._toggleShapePointActive(pointNode, false);
    }

    if (point !== null && point !== undefined) {
      // Because one inset value can represent multiple points, inset points use classes
      // instead of data.
      selector = (INSET_POINT_TYPES.includes(point)) ?
                 `.ruleview-shape-point.${point}` :
                 `.ruleview-shape-point[data-point='${point}']`;

      for (let pointNode of ruleViewEl.querySelectorAll(selector)) {
        let nodeInfo = view.getNodeInfo(pointNode);
        if (this.isRuleViewShapePoint(nodeInfo)) {
          this._toggleShapePointActive(pointNode, true);
        }
      }
    }
  }

  /**
   * Toggle the flexbox highlighter for the given flexbox container element.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the flexbox container element to highlight.
   * @param  {Object} options
   *         Object used for passing options to the flexbox highlighter.
   */
  async toggleFlexboxHighlighter(node, options = {}) {
    if (node == this.flexboxHighlighterShown) {
      await this.hideFlexboxHighlighter(node);
      return;
    }

    await this.showFlexboxHighlighter(node, options);
  }

  /**
   * Show the flexbox highlighter for the given flexbox container element.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the flexbox container element to highlight.
   * @param  {Object} options
   *         Object used for passing options to the flexbox highlighter.
   */
  async showFlexboxHighlighter(node, options) {
    let highlighter = await this._getHighlighter("FlexboxHighlighter");
    if (!highlighter) {
      return;
    }

    let isShown = await highlighter.show(node, options);
    if (!isShown) {
      return;
    }

    this._toggleRuleViewIcon(node, true, ".ruleview-flex");

    try {
      // Save flexbox highlighter state.
      let { url } = this.inspector.target;
      let selector = await node.getUniqueSelector();
      this.state.flexbox = { selector, options, url };
      this.flexboxHighlighterShown = node;

      // Emit the NodeFront of the flexbox container element that the flexbox highlighter
      // was shown for.
      this.emit("flexbox-highlighter-shown", node, options);
    } catch (e) {
      this._handleRejection(e);
    }
  }

  /**
   * Hide the flexbox highlighter for the given flexbox container element.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the flexbox container element to unhighlight.
   */
  async hideFlexboxHighlighter(node) {
    if (!this.flexboxHighlighterShown || !this.highlighters.FlexboxHighlighter) {
      return;
    }

    this._toggleRuleViewIcon(node, false, ".ruleview-flex");

    await this.highlighters.FlexboxHighlighter.hide();

    // Emit the NodeFront of the flexbox container element that the flexbox highlighter
    // was hidden for.
    this.emit("flexbox-highlighter-hidden", this.flexboxHighlighterShown);
    this.flexboxHighlighterShown = null;

    // Erase flexbox highlighter state.
    this.state.flexbox = null;
  }

  /**
   * Toggle the grid highlighter for the given grid container element.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the grid container element to highlight.
   * @param  {Object} options
   *         Object used for passing options to the grid highlighter.
   * @param. {String|null} trigger
   *         String name matching "grid" or "rule" to indicate where the
   *         grid highlighter was toggled on from. "grid" represents the grid view
   *         "rule" represents the rule view.
   */
  async toggleGridHighlighter(node, options = {}, trigger) {
    if (node == this.gridHighlighterShown) {
      await this.hideGridHighlighter(node);
      return;
    }

    await this.showGridHighlighter(node, options, trigger);
  }

  /**
   * Show the grid highlighter for the given grid container element.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the grid container element to highlight.
   * @param  {Object} options
   *         Object used for passing options to the grid highlighter.
   */
  async showGridHighlighter(node, options, trigger) {
    let highlighter = await this._getHighlighter("CssGridHighlighter");
    if (!highlighter) {
      return;
    }

    let isShown = await highlighter.show(node, options);
    if (!isShown) {
      return;
    }

    this._toggleRuleViewIcon(node, true, ".ruleview-grid");

    if (trigger == "grid") {
      Services.telemetry.scalarAdd("devtools.grid.gridinspector.opened", 1);
    } else if (trigger == "rule") {
      Services.telemetry.scalarAdd("devtools.rules.gridinspector.opened", 1);
    }

    try {
      // Save grid highlighter state.
      let { url } = this.inspector.target;
      let selector = await node.getUniqueSelector();
      this.state.grid = { selector, options, url };
      this.gridHighlighterShown = node;
      // Emit the NodeFront of the grid container element that the grid highlighter was
      // shown for.
      this.emit("grid-highlighter-shown", node, options);
    } catch (e) {
      this._handleRejection(e);
    }
  }

  /**
   * Hide the grid highlighter for the given grid container element.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the grid container element to unhighlight.
   */
  async hideGridHighlighter(node) {
    if (!this.gridHighlighterShown || !this.highlighters.CssGridHighlighter) {
      return;
    }

    this._toggleRuleViewIcon(node, false, ".ruleview-grid");

    await this.highlighters.CssGridHighlighter.hide();

    // Emit the NodeFront of the grid container element that the grid highlighter was
    // hidden for.
    this.emit("grid-highlighter-hidden", this.gridHighlighterShown,
      this.state.grid.options);
    this.gridHighlighterShown = null;

    // Erase grid highlighter state.
    this.state.grid = {};
  }

  /**
   * Toggle the geometry editor highlighter for the given element.
   *
   * @param {NodeFront} node
   *        The NodeFront of the element to highlight.
   */
  async toggleGeometryHighlighter(node) {
    if (node == this.geometryEditorHighlighterShown) {
      await this.hideGeometryEditor();
      return;
    }

    await this.showGeometryEditor(node);
  }

  /**
   * Show the geometry editor highlightor for the given element.
   *
   * @param {NodeFront} node
   *        THe NodeFront of the element to highlight.
   */
  async showGeometryEditor(node) {
    let highlighter = await this._getHighlighter("GeometryEditorHighlighter");
    if (!highlighter) {
      return;
    }

    let isShown = await highlighter.show(node);
    if (!isShown) {
      return;
    }

    this.emit("geometry-editor-highlighter-shown");
    this.geometryEditorHighlighterShown = node;
  }

  /**
   * Hide the geometry editor highlighter.
   */
  async hideGeometryEditor() {
    if (!this.geometryEditorHighlighterShown ||
        !this.highlighters.GeometryEditorHighlighter) {
      return;
    }

    await this.highlighters.GeometryEditorHighlighter.hide();

    this.emit("geometry-editor-highlighter-hidden");
    this.geometryEditorHighlighterShown = null;
  }

  /**
   * Handle events emitted by the highlighter.
   *
   * @param {Object} data
   *        The data object sent in the event.
   */
  _onHighlighterEvent(data) {
    if (data.type === "shape-hover-on") {
      this.state.shapes.hoverPoint = data.point;
      this.emit("hover-shape-point", data.point);
    } else if (data.type === "shape-hover-off") {
      this.state.shapes.hoverPoint = null;
      this.emit("hover-shape-point", null);
    }

    this.emit("highlighter-event-handled");
  }

  /**
   * Restores the saved flexbox highlighter state.
   */
  async restoreFlexboxState() {
    try {
      await this.restoreState("flexbox", this.state.flexbox, this.showFlexboxHighlighter);
    } catch (e) {
      this._handleRejection(e);
    }
  }

  /**
   * Restores the saved grid highlighter state.
   */
  async restoreGridState() {
    try {
      await this.restoreState("grid", this.state.grid, this.showGridHighlighter);
    } catch (e) {
      this._handleRejection(e);
    }
  }

  /**
   * Restores the saved shape highlighter state.
   */
  async restoreShapeState() {
    try {
      await this.restoreState("shapes", this.state.shapes, this.showShapesHighlighter);
    } catch (e) {
      this._handleRejection(e);
    }
  }

  /**
   * Helper function called by restoreFlexboxState, restoreGridState and
   * restoreShapeState. Restores the saved highlighter state for the given highlighter
   * and their state.
   *
   * @param  {String} name
   *         The name of the highlighter to be restored
   * @param  {Object} state
   *         The state of the highlighter to be restored
   * @param  {Function} showFunction
   *         The function that shows the highlighter
   * @return {Promise} that resolves when the highlighter state was restored, and the
   *         expected highlighters are displayed.
   */
  async restoreState(name, state, showFunction) {
    let { selector, options, url } = state;

    if (!selector || url !== this.inspector.target.url) {
      // Bail out if no selector was saved, or if we are on a different page.
      this.emit(`${name}-state-restored`, { restored: false });
      return;
    }

    let walker = this.inspector.walker;
    let rootNode = await walker.getRootNode();
    let nodeFront = await walker.querySelector(rootNode, selector);

    if (nodeFront) {
      if (options.hoverPoint) {
        options.hoverPoint = null;
      }

      await showFunction(nodeFront, options);
      this.emit(`${name}-state-restored`, { restored: true });
    }

    this.emit(`${name}-state-restored`, { restored: false });
  }

  /**
   * Get a highlighter front given a type. It will only be initialized once.
   *
   * @param  {String} type
   *         The highlighter type. One of this.highlighters.
   * @return {Promise} that resolves to the highlighter
   */
  async _getHighlighter(type) {
    let utils = this.highlighterUtils;

    if (this.highlighters[type]) {
      return this.highlighters[type];
    }

    let highlighter;

    try {
      highlighter = await utils.getHighlighterByType(type);
    } catch (e) {
      // Ignore any error
    }

    if (!highlighter) {
      return null;
    }

    highlighter.on("highlighter-event", this._onHighlighterEvent);
    this.highlighters[type] = highlighter;
    return highlighter;
  }

  _handleRejection(error) {
    if (!this.destroyed) {
      console.error(error);
    }
  }

  /**
   * Toggle all the icons with the given selector in the rule view if the current
   * inspector selection is the highlighted node.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the element with a shape to highlight.
   * @param  {Boolean} active
   *         Whether or not the shape icon should be active.
   * @param  {String} selector
   *         The selector of the rule view icon to toggle.
   */
  _toggleRuleViewIcon(node, active, selector) {
    if (this.inspector.selection.nodeFront != node) {
      return;
    }

    let ruleViewEl = this.inspector.getPanel("ruleview").view.element;

    for (let icon of ruleViewEl.querySelectorAll(selector)) {
      icon.classList.toggle("active", active);
    }
  }

  /**
   * Toggle the class "active" on the given shape point in the rule view if the current
   * inspector selection is highlighted by the shapes highlighter.
   *
   * @param {NodeFront} node
   *        The NodeFront of the shape point to toggle
   * @param {Boolean} active
   *        Whether the shape point should be active
   */
  _toggleShapePointActive(node, active) {
    if (this.inspector.selection.nodeFront != this.shapesHighlighterShown) {
      return;
    }

    node.classList.toggle("active", active);
  }

  /**
   * Hide the currently shown hovered highlighter.
   */
  _hideHoveredHighlighter() {
    if (!this.hoveredHighlighterShown ||
        !this.highlighters[this.hoveredHighlighterShown]) {
      return;
    }

    // For some reason, the call to highlighter.hide doesn't always return a
    // promise. This causes some tests to fail when trying to install a
    // rejection handler on the result of the call. To avoid this, check
    // whether the result is truthy before installing the handler.
    let onHidden = this.highlighters[this.hoveredHighlighterShown].hide();
    if (onHidden) {
      onHidden.catch(console.error);
    }

    this.hoveredHighlighterShown = null;
    this.emit("highlighter-hidden");
  }

  /**
   * Given a node front and a function that hides the given node's highlighter, hides
   * the highlighter if the node front is no longer in the DOM tree. This is called
   * from the "markupmutation" event handler.
   *
   * @param  {NodeFront} node
   *         The NodeFront of a highlighted DOM node.
   * @param  {Function} hideHighlighter
   *         The function that will hide the highlighter of the highlighted node.
   */
  async _hideHighlighterIfDeadNode(node, hideHighlighter) {
    if (!node) {
      return;
    }

    try {
      let isInTree = await this.inspector.walker.isInDOMTree(node);
      if (!isInTree) {
        hideHighlighter(node);
      }
    } catch (e) {
      console.error(e);
    }
  }

  /**
   * Is the current hovered node a css transform property value in the
   * computed-view.
   *
   * @param  {Object} nodeInfo
   * @return {Boolean}
   */
  _isComputedViewTransform(nodeInfo) {
    if (nodeInfo.view != "computed") {
      return false;
    }
    return nodeInfo.type === VIEW_NODE_VALUE_TYPE &&
           nodeInfo.value.property === "transform";
  }

  /**
   * Is the current clicked node a flex display property value in the
   * rule-view.
   *
   * @param  {DOMNode} node
   * @return {Boolean}
   */
  _isRuleViewDisplayFlex(node) {
    return this.isRuleView(node) && node.classList.contains("ruleview-flex");
  }

  /**
   * Is the current clicked node a grid display property value in the
   * rule-view.
   *
   * @param  {DOMNode} node
   * @return {Boolean}
   */
  _isRuleViewDisplayGrid(node) {
    return this.isRuleView(node) && node.classList.contains("ruleview-grid");
  }

  /**
   * Does the current clicked node have the shapes highlighter toggle in the
   * rule-view.
   *
   * @param  {DOMNode} node
   * @return {Boolean}
   */
  _isRuleViewShape(node) {
    return this.isRuleView(node) && node.classList.contains("ruleview-shape");
  }

  /**
   * Is the current hovered node a css transform property value in the rule-view.
   *
   * @param  {Object} nodeInfo
   * @return {Boolean}
   */
  _isRuleViewTransform(nodeInfo) {
    if (nodeInfo.view != "rule") {
      return false;
    }
    let isTransform = nodeInfo.type === VIEW_NODE_VALUE_TYPE &&
                      nodeInfo.value.property === "transform";
    let isEnabled = nodeInfo.value.enabled &&
                    !nodeInfo.value.overridden &&
                    !nodeInfo.value.pseudoElement;
    return isTransform && isEnabled;
  }

  /**
   * Is the current hovered node a highlightable shape point in the rule-view.
   *
   * @param  {Object} nodeInfo
   * @return {Boolean}
   */
  isRuleViewShapePoint(nodeInfo) {
    if (nodeInfo.view != "rule") {
      return false;
    }
    let isShape = nodeInfo.type === VIEW_NODE_SHAPE_POINT_TYPE &&
                  (nodeInfo.value.property === "clip-path" ||
                  nodeInfo.value.property === "shape-outside");
    let isEnabled = nodeInfo.value.enabled &&
                    !nodeInfo.value.overridden &&
                    !nodeInfo.value.pseudoElement;
    return isShape && isEnabled && nodeInfo.value.toggleActive &&
           !this.state.shapes.options.transformMode;
  }

  onClick(event) {
    if (this._isRuleViewDisplayGrid(event.target)) {
      event.stopPropagation();

      let { store } = this.inspector;
      let { grids, highlighterSettings } = store.getState();
      let grid = grids.find(g => g.nodeFront == this.inspector.selection.nodeFront);

      highlighterSettings.color = grid ? grid.color : DEFAULT_GRID_COLOR;

      this.toggleGridHighlighter(this.inspector.selection.nodeFront, highlighterSettings,
        "rule");
    } else if (this._isRuleViewDisplayFlex(event.target)) {
      event.stopPropagation();

      this.toggleFlexboxHighlighter(this.inspector.selection.nodeFront);
    } else if (this._isRuleViewShape(event.target)) {
      event.stopPropagation();

      let settings = {
        mode: event.target.dataset.mode,
        ctrlOrMetaPressed: event.metaKey || event.ctrlKey
      };
      this.toggleShapesHighlighter(this.inspector.selection.nodeFront, settings);
    }
  }

  onMouseMove(event) {
    // Bail out if the target is the same as for the last mousemove.
    if (event.target === this._lastHovered) {
      return;
    }

    // Only one highlighter can be displayed at a time, hide the currently shown.
    this._hideHoveredHighlighter();

    this._lastHovered = event.target;

    let view = this.isRuleView(this._lastHovered) ?
      this.inspector.getPanel("ruleview").view :
      this.inspector.getPanel("computedview").computedView;
    let nodeInfo = view.getNodeInfo(event.target);
    if (!nodeInfo) {
      return;
    }

    if (this.isRuleViewShapePoint(nodeInfo)) {
      let { point } = nodeInfo.value;
      this.hoverPointShapesHighlighter(this.inspector.selection.nodeFront, point);
      this.emit("hover-shape-point", point);
      return;
    }

    // Choose the type of highlighter required for the hovered node.
    let type;
    if (this._isRuleViewTransform(nodeInfo) ||
        this._isComputedViewTransform(nodeInfo)) {
      type = "CssTransformHighlighter";
    }

    if (type) {
      this.hoveredHighlighterShown = type;
      let node = this.inspector.selection.nodeFront;
      this._getHighlighter(type)
          .then(highlighter => highlighter.show(node))
          .then(shown => {
            if (shown) {
              this.emit("highlighter-shown");
            }
          });
    }
  }

  onMouseOut(event) {
    // Only hide the highlighter if the mouse leaves the currently hovered node.
    if (!this._lastHovered ||
        (event && this._lastHovered.contains(event.relatedTarget))) {
      return;
    }

    // Otherwise, hide the highlighter.
    let view = this.isRuleView(this._lastHovered) ?
      this.inspector.getPanel("ruleview").view :
      this.inspector.getPanel("computedview").computedView;
    let nodeInfo = view.getNodeInfo(this._lastHovered);
    if (nodeInfo && this.isRuleViewShapePoint(nodeInfo)) {
      this.hoverPointShapesHighlighter(this.inspector.selection.nodeFront, null);
      this.emit("hover-shape-point", null);
    }
    this._lastHovered = null;
    this._hideHoveredHighlighter();
  }

  /**
   * Handler function for "markupmutation" events. Hides the flexbox/grid/shapes
   * highlighter if the flexbox/grid/shapes container is no longer in the DOM tree.
   */
  async onMarkupMutation(evt, mutations) {
    let hasInterestingMutation = mutations.some(mut => mut.type === "childList");
    if (!hasInterestingMutation) {
      // Bail out if the mutations did not remove nodes, or if no grid highlighter is
      // displayed.
      return;
    }

    this._hideHighlighterIfDeadNode(this.flexboxHighlighterShown,
      this.hideFlexboxHighlighter);
    this._hideHighlighterIfDeadNode(this.gridHighlighterShown,
      this.hideGridHighlighter);
    this._hideHighlighterIfDeadNode(this.shapesHighlighterShown,
      this.hideShapesHighlighter);
  }

  /**
   * Clear saved highlighter shown properties on will-navigate.
   */
  onWillNavigate() {
    this.flexboxHighlighterShown = null;
    this.geometryEditorHighlighterShown = null;
    this.gridHighlighterShown = null;
    this.hoveredHighlighterShown = null;
    this.selectorHighlighterShown = null;
    this.shapesHighlighterShown = null;
  }

  /**
   * Destroy this overlay instance, removing it from the view and destroying
   * all initialized highlighters.
   */
  destroy() {
    for (let type in this.highlighters) {
      if (this.highlighters[type]) {
        if (this.highlighters[type].off) {
          this.highlighters[type].off("highlighter-event", this._onHighlighterEvent);
        }
        this.highlighters[type].finalize();
        this.highlighters[type] = null;
      }
    }

    // Remove inspector events.
    this.inspector.off("markupmutation", this.onMarkupMutation);
    this.inspector.target.off("will-navigate", this.onWillNavigate);

    this._lastHovered = null;

    this.inspector = null;
    this.highlighters = null;
    this.highlighterUtils = null;
    this.supportsHighlighters = null;
    this.state = null;

    this.flexboxHighlighterShown = null;
    this.geometryEditorHighlighterShown = null;
    this.gridHighlighterShown = null;
    this.hoveredHighlighterShown = null;
    this.selectorHighlighterShown = null;
    this.shapesHighlighterShown = null;

    this.destroyed = true;
  }
}

module.exports = HighlightersOverlay;
