/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const { debounce } = require("devtools/shared/debounce");

/**
 * The ShapesInContextEditor:
 * - communicates with the ShapesHighlighter actor from the server;
 * - listens to events for shape change and hover point coming from the shape-highlighter;
 * - writes shape value changes to the CSS declaration it was triggered from;
 * - synchronises highlighting coordinate points on mouse over between the shapes
 *   highlighter and the shape value shown in the Rule view.
 *
 * It is instantiated once in HighlightersOverlay by calls to .getInContextEditor().
 */
class ShapesInContextEditor {
  constructor(highlighter, inspector, state) {
    EventEmitter.decorate(this);

    this.inspector = inspector;
    this.highlighter = highlighter;
    // Refence to the NodeFront currently being highlighted.
    this.highlighterTargetNode = null;
    this.highligherEventHandlers = {};
    this.highligherEventHandlers["shape-change"] = this.onShapeChange;
    this.highligherEventHandlers["shape-hover-on"] = this.onShapeHover;
    this.highligherEventHandlers["shape-hover-off"] = this.onShapeHover;
    // Mode for shapes highlighter: shape-outside or clip-path. Used to discern
    // when toggling the highlighter on the same node for different CSS properties.
    this.mode = null;
    // Reference to Rule view used to listen for changes
    this.ruleView = this.inspector.getPanel("ruleview").view;
    // Reference of |state| from HighlightersOverlay.
    this.state = state;
    // Reference to DOM node of the toggle icon for shapes highlighter.
    this.swatch = null;
    // Reference to TextProperty where shape changes will be written.
    this.textProperty = null;

    // Commit triggers expensive DOM changes in TextPropertyEditor.update()
    // so we debounce it.
    this.commit = debounce(this.commit, 200, this);
    this.onHighlighterEvent = this.onHighlighterEvent.bind(this);
    this.onNodeFrontChanged = this.onNodeFrontChanged.bind(this);
    this.onShapeValueUpdated = this.onShapeValueUpdated.bind(this);
    this.onRuleViewChanged = this.onRuleViewChanged.bind(this);

    this.highlighter.on("highlighter-event", this.onHighlighterEvent);
    this.ruleView.on("ruleview-changed", this.onRuleViewChanged);
  }

  /**
  * Called when the element style changes from the Rule view.
  * If the TextProperty we're acting on isn't enabled anymore or overridden,
  * turn off the shapes highlighter.
  */
  async onRuleViewChanged() {
    if (this.textProperty &&
      (!this.textProperty.enabled || this.textProperty.overridden)) {
      await this.hide();
    }
  }

  /**
   * Toggle the shapes highlighter for the given element.
   *
   * @param {NodeFront} node
   *        The NodeFront of the element with a shape to highlight.
   * @param {Object} options
   *        Object used for passing options to the shapes highlighter.
   */
  async toggle(node, options, prop) {
    // Same target node, same mode -> hide and exit OR switch to toggle transform mode.
    if ((node == this.highlighterTargetNode) && (this.mode === options.mode)) {
      if (!options.transformMode) {
        await this.hide();
        return;
      }

      options.transformMode = !this.state.shapes.options.transformMode;
    }

    // Same target node, dfferent modes -> toggle between shape-outside and clip-path.
    // Hide highlighter for previous property, but continue and show for other property.
    if ((node == this.highlighterTargetNode) && (this.mode !== options.mode)) {
      await this.hide();
    }

    this.textProperty = prop;
    this.findSwatch();
    await this.show(node, options);
  }

  /**
   * Show the shapes highlighter for the given element.
   *
   * @param {NodeFront} node
   *        The NodeFront of the element with a shape to highlight.
   * @param {Object} options
   *        Object used for passing options to the shapes highlighter.
   */
  async show(node, options) {
    const isShown = await this.highlighter.show(node, options);
    if (!isShown) {
      return;
    }

    this.inspector.selection.on("detached-front", this.onNodeFrontChanged);
    this.inspector.selection.on("new-node-front", this.onNodeFrontChanged);
    this.ruleView.on("property-value-updated", this.onShapeValueUpdated);
    this.highlighterTargetNode = node;
    this.mode = options.mode;
    this.emit("show", { node, options });
  }

  /**
   * Hide the shapes highlighter.
   */
  async hide() {
    try {
      await this.highlighter.hide();
    } catch (err) {
      // silent error
    }

    if (this.swatch) {
      this.swatch.classList.remove("active");
    }
    this.swatch = null;
    this.textProperty = null;

    this.emit("hide", { node: this.highlighterTargetNode });
    this.inspector.selection.off("detached-front", this.onNodeFrontChanged);
    this.inspector.selection.off("new-node-front", this.onNodeFrontChanged);
    this.ruleView.off("property-value-updated", this.onShapeValueUpdated);
    this.highlighterTargetNode = null;
  }

  /**
   * Identify the swatch (aka toggle icon) DOM node from the TextPropertyEditor of the
   * TextProperty we're working with. Whenever the TextPropertyEditor is updated (i.e.
   * when committing the shape value to the Rule view), it rebuilds its DOM and the old
   * swatch reference becomes invalid. Call this method to identify the current swatch.
   */
  findSwatch() {
    const valueSpan = this.textProperty.editor.valueSpan;
    this.swatch = valueSpan.querySelector(".ruleview-shapeswatch");
    if (this.swatch) {
      this.swatch.classList.add("active");
    }
  }

  /**
   * Handle events emitted by the highlighter.
   * Find any callback assigned to the event type and call it with the given data object.
   *
   * @param {Object} data
   *        The data object sent in the event.
   */
  onHighlighterEvent(data) {
    const handler = this.highligherEventHandlers[data.type];
    if (!handler || typeof handler !== "function") {
      return;
    }
    handler.call(this, data);
    this.inspector.highlighters.emit("highlighter-event-handled");
  }

  /**
  * Clean up when node selection changes because Rule view and TextPropertyEditor
  * instances are not automatically destroyed when selection changes.
  */
  async onNodeFrontChanged() {
    try {
      await this.hide();
    } catch (err) {
      // Silent error.
    }
  }

  /**
  * Handler for "shape-change" event from the shapes highlighter.
  *
  * @param  {Object} data
  *         Data associated with the "shape-change" event.
  *         Contains:
  *         - {String} value: the new shape value.
  *         - {String} type: the event type ("shape-change").
  */
  onShapeChange(data) {
    this.preview(data.value);
    this.commit(data.value);
  }

  /**
  * Handler for "shape-hover-on" and "shape-hover-off" events from the shapes highlighter.
  * Called when the mouse moves over or off of a coordinate point inside the shapes
  * highlighter. Marks/unmarks the corresponding coordinate node in the shape value
  * from the Rule view.
  *
  * @param  {Object} data
  *         Data associated with the "shape-hover" event.
  *         Contains:
  *         - {String|null} point: coordinate to highlight or null if nothing to highlight
  *         - {String} type: the event type ("shape-hover-on" or "shape-hover-on").
  */
  onShapeHover(data) {
    const shapeValueEl = this.swatch && this.swatch.nextSibling;
    if (!shapeValueEl) {
      return;
    }

    const pointSelector = ".ruleview-shape-point";
    // First, unmark all highlighted coordinate nodes from Rule view
    for (const node of shapeValueEl.querySelectorAll(`${pointSelector}.active`)) {
      node.classList.remove("active");
    }

    // Exit if there's no coordinate to highlight.
    if (typeof data.point !== "string") {
      return;
    }

    const point = (data.point.includes(",")) ? data.point.split(",")[0] : data.point;

    /**
    * Build selector for coordinate nodes in shape value that must be highlighted.
    * Coordinate values for inset() use class names instead of data attributes because
    * a single node may represent multiple coordinates in shorthand notation.
    * Example: inset(50px); The node wrapping 50px represents all four inset coordinates.
    */
    const INSET_POINT_TYPES = ["top", "right", "bottom", "left"];
    const selector = INSET_POINT_TYPES.includes(point) ?
                  `${pointSelector}.${point}` :
                  `${pointSelector}[data-point='${point}']`;

    for (const node of shapeValueEl.querySelectorAll(selector)) {
      node.classList.add("active");
    }
  }

  /**
  * Handler for "property-value-updated" event triggered by the Rule view.
  * Called after the shape value has been written to the element's style and the Rule
  * view updated. Emits an event on HighlightersOverlay that is expected by
  * tests in order to check if the shape value has been correctly applied.
  */
  onShapeValueUpdated() {
    // When TextPropertyEditor updates, it replaces the previous swatch DOM node.
    // Find and store the new one.
    this.findSwatch();
    this.inspector.highlighters.emit("shapes-highlighter-changes-applied");
  }

  /**
  * Preview a shape value on the element without committing the changes to the Rule view.
  *
  * @param {String} value
  *        The shape value to set the current property to
  */
  preview(value) {
    if (!this.textProperty) {
      return;
    }
    // Update the element's style to see live results.
    this.textProperty.rule.previewPropertyValue(this.textProperty, value);
    // Update the text of CSS value in the Rule view. This makes it inert.
    // When commit() is called, the value is reparsed and its DOM structure rebuilt.
    this.swatch.nextSibling.textContent = value;
  }

  /**
  * Commit a shape value change which triggers an expensive operation that rebuilds
  * part of the DOM of the TextPropertyEditor. Called in a debounced manner; see
  * constructor.
  *
  * @param {String} value
  *        The shape value for the current property
  */
  commit(value) {
    if (!this.textProperty) {
      return;
    }

    this.textProperty.setValue(value);
  }

  destroy() {
    this.highlighter.off("highlighter-event", this.onHighlighterEvent);
    this.ruleView.off("ruleview-changed", this.onRuleViewChanged);
    this.highligherEventHandlers = {};
  }
}

module.exports = ShapesInContextEditor;
