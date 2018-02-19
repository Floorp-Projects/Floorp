/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const { debounce } = require("devtools/shared/debounce");

/**
 * The ShapesInContextEditor:
 * - communicates with the ShapesHighlighter actor from the server;
 * - triggers the shapes highlighter on click on a swatch element (aka toggle icon)
 *   in the Rule view next to CSS properties like `shape-outside` and `clip-path`;
 * - listens to events for shape change and hover point coming from the shape-highlighter;
 * - writes shape value changes to the CSS declaration it was triggered from;
 * - synchroninses highlighting coordinate points on mouse over between the shapes
 *   highlighter and the shape value shown in the Rule view.
 *
 * It behaves like a singleton, though it is not yet implemented like that.
 * It is instantiated once in HighlightersOverlay by calls to .getInContextEditor().
 * It is requested by TextPropertyEditor instances for `clip-path` and `shape-outside`
 * CSS properties to register swatches that should trigger the shapes highlighter and to
 * which shape values should be written back.
 */
class ShapesInContextEditor {
  constructor(highlighter, inspector, state) {
    EventEmitter.decorate(this);

    this.activeSwatch = null;
    this.activeProperty = null;
    this.inspector = inspector;
    this.highlighter = highlighter;
    // Refence to the NodeFront currently being highlighted.
    this.highlighterTargetNode = null;
    this.highligherEventHandlers = {};
    this.highligherEventHandlers["shape-change"] = this.onShapeChange;
    this.highligherEventHandlers["shape-hover-on"] = this.onShapeHover;
    this.highligherEventHandlers["shape-hover-off"] = this.onShapeHover;
    // Map with data required to preview and persist shape value changes to the Rule view.
    // keys - TextProperty instances relevant for shape editor (clip-path, shape-outside).
    // values - objects with references to swatch elements that trigger the shape editor
    //          and callbacks used to preview and persist shape value changes.
    this.links = new Map();
    // Reference to Rule view used to listen for changes
    this.ruleView = this.inspector.getPanel("ruleview").view;
    // Reference of |state| from HighlightersOverlay.
    this.state = state;

    // Commit triggers expensive DOM changes in TextPropertyEditor.update()
    // so we debounce it.
    this.commit = debounce(this.commit, 200, this);
    this.onChangesApplied = this.onChangesApplied.bind(this);
    this.onHighlighterEvent = this.onHighlighterEvent.bind(this);
    this.onNodeFrontChanged = this.onNodeFrontChanged.bind(this);
    this.onRuleViewChanged = this.onRuleViewChanged.bind(this);
    this.onSwatchClick = this.onSwatchClick.bind(this);

    this.highlighter.on("highlighter-event", this.onHighlighterEvent);
    this.ruleView.on("ruleview-changed", this.onRuleViewChanged);
  }

  /**
  * The shapes in-context editor works by listening to shape value changes from the shapes
  * highlighter and mapping them to the correct CSS property in the Rule view.
  *
  * In order to know where to map changes, the TextPropertyEditor instances register
  * themselves in a map object internal to the ShapesInContextEditor.
  * Calling the `ShapesInContextEditor.link()` method, they provide:
  * - the TextProperty model to which shape value changes should map to (this is the key
  * in the internal map object);
  * - the swatch element that triggers the shapes highlighter,
  * - callbacks that must be used in order to preview and persist the shape value changes.
  *
  * When the TextPropertyEditor updates, it rebuilds part of its DOM and destroys the
  * original swatch element. Losing that reference to the swatch element breaks the
  * ability to show the indicator for the shape editor that is on and prevents the preview
  * functionality from working properly. Because of that, this link() method gets called
  * on every TextPropertyEditor.update() and, if the TextProperty model used as a key is
  * already registered, the old swatch element reference is replaced with the new one.
  *
  * @param {TextProperty} prop
  *        TextProperty instance for clip-path or shape-outside from Rule view for the
  *        selected element.
  * @param {Node} swatch
  *        Reference to DOM element next to shape value that toggles the shapes
  *        highlighter. This element is destroyed after each call to |this.commit()|
  *        because that rebuilds the DOM for the shape value in the Rule view.
  *        Repeated calls to this method with the same prop (TextProperty) will
  *        replace the swatch reference to the new element for consistent behaviour.
  * @param {Object} callbacks
  *        Collection of callbacks from the TextPropertyEditor:
  *        - onPreview: method called to preview a shape value on the element
  *        - onCommit: method called to commit a shape value to the Rule view.
  */
  link(prop, swatch, callbacks = {}) {
    if (this.links.has(prop)) {
      // Swatch element may have changed, replace with given reference.
      this.replaceSwatch(prop, swatch);
      return;
    }
    if (!callbacks.onPreview) {
      callbacks.onPreview = function() {};
    }
    if (!callbacks.onCommit) {
      callbacks.onCommit = function() {};
    }

    swatch.addEventListener("click", this.onSwatchClick);
    this.links.set(prop, { swatch, callbacks });

    // Event on HighlightersOverlay expected by tests to know when to click the swatch.
    this.inspector.highlighters.emit("shapes-highlighter-armed");
  }

  /**
  * Remove references to swatch and callbacks for the given TextProperty model so that
  * shape value changes cannot map back to it and the shape editor cannot be triggered
  * from its associated swatch element.
  *
  * @param {TextProperty} prop
  *        TextProperty instance from Rule view.
  */
  async unlink(prop) {
    let data = this.links.get(prop);
    if (!data || !data.swatch) {
      return;
    }
    if (this.activeProperty === prop) {
      await this.hide();
    }

    data.swatch.classList.remove("active");
    data.swatch.removeEventListener("click", this.onSwatchClick);
    this.links.delete(prop);
  }

  /**
  * Remove all linked references from TextPropertyEditor.
  */
  async unlinkAll() {
    for (let [prop] of this.links) {
      await this.unlink(prop);
    }
  }

  /**
  * If the given TextProperty exists as a key in |this.links|, replace the reference it
  * has to the swatch element with a new node reference.
  *
  * @param {TextProperty} prop
  *        TextProperty instance from Rule view.
  * @param {Node} swatch
  *        Reference to swatch DOM element.
  */
  replaceSwatch(prop, swatch) {
    let data = this.links.get(prop);
    if (data.swatch) {
      // Cleanup old
      data.swatch.removeEventListener("click", this.onSwatchClick);
      data.swatch = undefined;
      // Setup new
      swatch.addEventListener("click", this.onSwatchClick);
      data.swatch = swatch;
    }
  }

  /**
  * Check if the given swatch DOM element already exists in the collection of linked
  * swatches.
  *
  * @param {Node} swatch
  *        Reference to swatch DOM element.
  * @return {Boolean}
  *
  */
  hasSwatch(swatch) {
    for (let [, data] of this.links) {
      if (data.swatch == swatch) {
        return true;
      }
    }
    return false;
  }

  /**
  * Called when the element style changes from the Rule view.
  * If the TextProperty we're acting on isn't enabled anymore or overridden,
  * turn off the shapes highlighter.
  */
  async onRuleViewChanged() {
    if (this.activeProperty &&
      (!this.activeProperty.enabled || this.activeProperty.overridden)) {
      await this.hide();
    }
  }

  /**
  * Called when a swatch element is clicked. Toggles shapes highlighter to show or hide.
  * Sets the current swatch and corresponding TextProperty as the active ones. They will
  * be immediately unset if the toggle action is to hide the shapes highlighter.
  *
  * @param {MouseEvent} event
  *        Mouse click event.
  */
  onSwatchClick(event) {
    event.stopPropagation();
    for (let [prop, data] of this.links) {
      if (data.swatch == event.target) {
        this.activeSwatch = data.swatch;
        this.activeSwatch.classList.add("active");
        this.activeProperty = prop;
        break;
      }
    }

    let nodeFront = this.inspector.selection.nodeFront;
    let options =  {
      mode: event.target.dataset.mode,
      transformMode: event.metaKey || event.ctrlKey
    };

    this.toggle(nodeFront, options);
  }

  /**
   * Toggle the shapes highlighter for the given element.
   *
   * @param {NodeFront} node
   *        The NodeFront of the element with a shape to highlight.
   * @param {Object} options
   *        Object used for passing options to the shapes highlighter.
   */
  async toggle(node, options) {
    if (node == this.highlighterTargetNode) {
      if (!options.transformMode) {
        await this.hide();
        return;
      }

      options.transformMode = !this.state.shapes.options.transformMode;
    }

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
    let isShown = await this.highlighter.show(node, options);
    if (!isShown) {
      return;
    }

    this.inspector.selection.on("detached-front", this.onNodeFrontChanged);
    this.inspector.selection.on("new-node-front", this.onNodeFrontChanged);
    this.highlighterTargetNode = node;
    this.emit("show", { node, options });
  }

  /**
   * Hide the shapes highlighter.
   */
  async hide() {
    await this.highlighter.hide();

    if (this.activeSwatch) {
      this.activeSwatch.classList.remove("active");
    }
    this.activeSwatch = null;
    this.activeProperty = null;

    this.emit("hide", { node: this.highlighterTargetNode });
    this.inspector.selection.off("detached-front", this.onNodeFrontChanged);
    this.inspector.selection.off("new-node-front", this.onNodeFrontChanged);
    this.highlighterTargetNode = null;
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
      await this.unlinkAll();
    } catch (err) {
      // Silent error.
    }
  }

  /**
  * Called when there's an updated shape value from the highlighter.
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
  * Called when the mouse moves on or off of a coordinate point inside the shapes
  * highlighter and marks/unmarks the corresponding coordinate node in the shape value
  * from the Rule view.
  *
  * @param  {Object} data
  *         Data associated with the "shape-hover" event.
  *         Contains:
  *         - {String|null} point: coordinate to highlight or null if nothing to highlight
  *         - {String} type: the event type ("shape-hover-on" or "shape-hover-on").
  */
  onShapeHover(data) {
    if (!this.activeProperty) {
      return;
    }

    let shapeValueEl = this.links.get(this.activeProperty).swatch.nextSibling;
    if (!shapeValueEl) {
      return;
    }
    let pointSelector = ".ruleview-shape-point";
    // First, unmark all highlighted coordinate nodes from Rule view
    for (let node of shapeValueEl.querySelectorAll(`${pointSelector}.active`)) {
      node.classList.remove("active");
    }

    // Exit if there's no coordinate to highlight.
    if (typeof data.point !== "string") {
      return;
    }

    let point = (data.point.includes(",")) ? data.point.split(",")[0] : data.point;

    /**
    * Build selector for coordinate nodes in shape value that must be highlighted.
    * Coordinate values for inset() use class names instead of data attributes because
    * a single node may represent multiple coordinates in shorthand notation.
    * Example: inset(50px); The node wrapping 50px represents all four inset coordinates.
    */
    const INSET_POINT_TYPES = ["top", "right", "bottom", "left"];
    let selector = INSET_POINT_TYPES.includes(point) ?
                  `${pointSelector}.${point}` :
                  `${pointSelector}[data-point='${point}']`;

    for (let node of shapeValueEl.querySelectorAll(selector)) {
      node.classList.add("active");
    }
  }

  /**
  * Preview this shape value on the element but do commit the changes to the Rule view.
  *
  * @param {String} value
  *        The shape value to set the current property to
  */
  preview(value) {
    if (!this.activeProperty) {
      return;
    }
    let data = this.links.get(this.activeProperty);
    // Update the element's styles to see live results.
    data.callbacks.onPreview(value);
    // Update the text of CSS value in the Rule view. This makes it inert.
    // When .commit() is called, the value is reparsed and its DOM structure rebuilt.
    data.swatch.nextSibling.textContent = value;
  }

  /**
  * Commit this shape value change which triggers an expensive operation that rebuilds
  * part of the DOM of the TextPropertyEditor. Called in a debounced manner.
  *
  * @param {String} value
  *        The shape value to set the current property to
  */
  commit(value) {
    if (!this.activeProperty) {
      return;
    }
    this.ruleView.once("ruleview-changed", this.onChangesApplied);
    let data = this.links.get(this.activeProperty);
    data.callbacks.onCommit(value);
  }

  /**
  * Called once after the shape value has been written to the element's style an Rule
  * view updated. Triggers an event on the HighlightersOverlay that is listened to by
  * tests in order to check if the shape value has been correctly applied.
  */
  onChangesApplied() {
    this.inspector.highlighters.emit("shapes-highlighter-changes-applied");
  }

  async destroy() {
    await this.hide();
    await this.unlinkAll();
    this.highlighter.off("highlighter-event", this.onHighlighterEvent);
    this.ruleView.off("ruleview-changed", this.onRuleViewChanged);
    this.highligherEventHandlers = {};
  }
}

module.exports = ShapesInContextEditor;
