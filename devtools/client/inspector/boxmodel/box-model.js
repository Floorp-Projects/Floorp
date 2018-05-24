/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  updateGeometryEditorEnabled,
  updateLayout,
  updateOffsetParent,
} = require("./actions/box-model");

loader.lazyRequireGetter(this, "EditingSession", "devtools/client/inspector/boxmodel/utils/editing-session");
loader.lazyRequireGetter(this, "InplaceEditor", "devtools/client/shared/inplace-editor", true);
loader.lazyRequireGetter(this, "getCssProperties", "devtools/shared/fronts/css-properties", true);

const NUMERIC = /^-?[\d\.]+$/;

/**
 * A singleton instance of the box model controllers.
 *
 * @param  {Inspector} inspector
 *         An instance of the Inspector currently loaded in the toolbox.
 * @param  {Window} window
 *         The document window of the toolbox.
 */
function BoxModel(inspector, window) {
  this.document = window.document;
  this.inspector = inspector;
  this.store = inspector.store;

  this.updateBoxModel = this.updateBoxModel.bind(this);

  this.onHideBoxModelHighlighter = this.onHideBoxModelHighlighter.bind(this);
  this.onHideGeometryEditor = this.onHideGeometryEditor.bind(this);
  this.onMarkupViewLeave = this.onMarkupViewLeave.bind(this);
  this.onMarkupViewNodeHover = this.onMarkupViewNodeHover.bind(this);
  this.onNewSelection = this.onNewSelection.bind(this);
  this.onShowBoxModelEditor = this.onShowBoxModelEditor.bind(this);
  this.onShowBoxModelHighlighter = this.onShowBoxModelHighlighter.bind(this);
  this.onSidebarSelect = this.onSidebarSelect.bind(this);
  this.onToggleGeometryEditor = this.onToggleGeometryEditor.bind(this);

  this.inspector.selection.on("new-node-front", this.onNewSelection);
  this.inspector.sidebar.on("select", this.onSidebarSelect);
}

BoxModel.prototype = {

  /**
   * Destruction function called when the inspector is destroyed. Removes event listeners
   * and cleans up references.
   */
  destroy() {
    this.inspector.selection.off("new-node-front", this.onNewSelection);
    this.inspector.sidebar.off("select", this.onSidebarSelect);

    this.untrackReflows();

    this._highlighters = null;
    this.document = null;
    this.inspector = null;
    this.walker = null;
  },

  get highlighters() {
    if (!this._highlighters) {
      // highlighters is a lazy getter in the inspector.
      this._highlighters = this.inspector.highlighters;
    }

    return this._highlighters;
  },

  /**
   * Returns an object containing the box model's handler functions used in the box
   * model's React component props.
   */
  getComponentProps() {
    return {
      onHideBoxModelHighlighter: this.onHideBoxModelHighlighter,
      onShowBoxModelEditor: this.onShowBoxModelEditor,
      onShowBoxModelHighlighter: this.onShowBoxModelHighlighter,
      onToggleGeometryEditor: this.onToggleGeometryEditor,
    };
  },

  /**
   * Returns true if the layout panel is visible, and false otherwise.
   */
  isPanelVisible() {
    return this.inspector.toolbox && this.inspector.sidebar &&
           this.inspector.toolbox.currentToolId === "inspector" &&
           (this.inspector.sidebar.getCurrentTabID() === "layoutview");
  },

  /**
   * Returns true if the layout panel is visible and the current element is valid to
   * be displayed in the view.
   */
  isPanelVisibleAndNodeValid() {
    return this.isPanelVisible() &&
           this.inspector.selection.isConnected() &&
           this.inspector.selection.isElementNode();
  },

  /**
   * Starts listening to reflows in the current tab.
   */
  trackReflows() {
    this.inspector.reflowTracker.trackReflows(this, this.updateBoxModel);
  },

  /**
   * Stops listening to reflows in the current tab.
   */
  untrackReflows() {
    this.inspector.reflowTracker.untrackReflows(this, this.updateBoxModel);
  },

  /**
   * Updates the box model panel by dispatching the new layout data.
   *
   * @param  {String} reason
   *         Optional string describing the reason why the boxmodel is updated.
   */
  updateBoxModel(reason) {
    this._updateReasons = this._updateReasons || [];
    if (reason) {
      this._updateReasons.push(reason);
    }

    let lastRequest = ((async function() {
      if (!this.inspector ||
          !this.isPanelVisible() ||
          !this.inspector.selection.isConnected() ||
          !this.inspector.selection.isElementNode()) {
        return null;
      }

      let node = this.inspector.selection.nodeFront;

      let layout = await this.inspector.pageStyle.getLayout(node, {
        autoMargins: true,
      });

      let styleEntries = await this.inspector.pageStyle.getApplied(node, {
        // We don't need styles applied to pseudo elements of the current node.
        skipPseudo: true
      });
      this.elementRules = styleEntries.map(e => e.rule);

      // Update the layout properties with whether or not the element's position is
      // editable with the geometry editor.
      let isPositionEditable = await this.inspector.pageStyle.isPositionEditable(node);

      layout = Object.assign({}, layout, {
        isPositionEditable,
      });

      const actorCanGetOffSetParent =
        await this.inspector.target.actorHasMethod("domwalker", "getOffsetParent");

      if (actorCanGetOffSetParent) {
        // Update the redux store with the latest offset parent DOM node
        let offsetParent = await this.inspector.walker.getOffsetParent(node);
        this.store.dispatch(updateOffsetParent(offsetParent));
      }

      // Update the redux store with the latest layout properties and update the box
      // model view.
      this.store.dispatch(updateLayout(layout));

      // If a subsequent request has been made, wait for that one instead.
      if (this._lastRequest != lastRequest) {
        return this._lastRequest;
      }

      this.inspector.emit("boxmodel-view-updated", this._updateReasons);

      this._lastRequest = null;
      this._updateReasons = [];

      return null;
    }).bind(this))().catch(console.error);

    this._lastRequest = lastRequest;
  },

  /**
   * Hides the box-model highlighter on the currently selected element.
   */
  onHideBoxModelHighlighter() {
    if (!this.inspector) {
      return;
    }

    let toolbox = this.inspector.toolbox;
    toolbox.highlighterUtils.unhighlight();
  },

  /**
   * Hides the geometry editor and updates the box moodel store with the new
   * geometry editor enabled state.
   */
  onHideGeometryEditor() {
    let { markup, selection, toolbox } = this.inspector;

    this.highlighters.hideGeometryEditor();
    this.store.dispatch(updateGeometryEditorEnabled(false));

    toolbox.off("picker-started", this.onHideGeometryEditor);
    selection.off("new-node-front", this.onHideGeometryEditor);
    markup.off("leave", this.onMarkupViewLeave);
    markup.off("node-hover", this.onMarkupViewNodeHover);
  },

  /**
   * Handler function that re-shows the geometry editor for an element that already
   * had the geometry editor enabled. This handler function is called on a "leave" event
   * on the markup view.
   */
  onMarkupViewLeave() {
    let state = this.store.getState();
    let enabled = state.boxModel.geometryEditorEnabled;

    if (!enabled) {
      return;
    }

    let nodeFront = this.inspector.selection.nodeFront;
    this.highlighters.showGeometryEditor(nodeFront);
  },

  /**
   * Handler function that temporarily hides the geomery editor when the
   * markup view has a "node-hover" event.
   */
  onMarkupViewNodeHover() {
    this.highlighters.hideGeometryEditor();
  },

  /**
   * Selection 'new-node-front' event handler.
   */
  onNewSelection() {
    if (!this.isPanelVisibleAndNodeValid()) {
      return;
    }

    if (this.inspector.selection.isConnected() &&
        this.inspector.selection.isElementNode()) {
      this.trackReflows();
    }

    this.updateBoxModel("new-selection");
  },

  /**
   * Shows the inplace editor when a box model editable value is clicked on the
   * box model panel.
   *
   * @param  {DOMNode} element
   *         The element that was clicked.
   * @param  {Event} event
   *         The event object.
   * @param  {String} property
   *         The name of the property.
   */
  onShowBoxModelEditor(element, event, property) {
    let session = new EditingSession({
      inspector: this.inspector,
      doc: this.document,
      elementRules: this.elementRules,
    });
    let initialValue = session.getProperty(property);

    let editor = new InplaceEditor({
      element: element,
      initial: initialValue,
      contentType: InplaceEditor.CONTENT_TYPES.CSS_VALUE,
      property: {
        name: property
      },
      start: self => {
        self.elt.parentNode.classList.add("boxmodel-editing");
      },
      change: value => {
        if (NUMERIC.test(value)) {
          value += "px";
        }

        let properties = [
          { name: property, value: value }
        ];

        if (property.substring(0, 7) == "border-") {
          let bprop = property.substring(0, property.length - 5) + "style";
          let style = session.getProperty(bprop);
          if (!style || style == "none" || style == "hidden") {
            properties.push({ name: bprop, value: "solid" });
          }
        }

        if (property.substring(0, 9) == "position-") {
          properties[0].name = property.substring(9);
        }

        session.setProperties(properties).catch(console.error);
      },
      done: (value, commit) => {
        editor.elt.parentNode.classList.remove("boxmodel-editing");
        if (!commit) {
          session.revert().then(() => {
            session.destroy();
          }, console.error);
          return;
        }

        this.updateBoxModel("editable-value-change");
      },
      cssProperties: getCssProperties(this.inspector.toolbox)
    }, event);
  },

  /**
   * Shows the box-model highlighter on the currently selected element.
   *
   * @param  {Object} options
   *         Options passed to the highlighter actor.
   */
  onShowBoxModelHighlighter(options = {}) {
    if (!this.inspector) {
      return;
    }

    let toolbox = this.inspector.toolbox;
    let nodeFront = this.inspector.selection.nodeFront;

    toolbox.highlighterUtils.highlightNodeFront(nodeFront, options);
  },

  /**
   * Handler for the inspector sidebar select event. Starts tracking reflows if the
   * layout panel is visible. Otherwise, stop tracking reflows. Finally, refresh the box
   * model view if it is visible.
   */
  onSidebarSelect() {
    if (!this.isPanelVisible()) {
      this.untrackReflows();
      return;
    }

    if (this.inspector.selection.isConnected() &&
        this.inspector.selection.isElementNode()) {
      this.trackReflows();
    }

    this.updateBoxModel();
  },

  /**
   * Toggles on/off the geometry editor for the current element when the geometry editor
   * toggle button is clicked.
   */
  onToggleGeometryEditor() {
    let { markup, selection, toolbox } = this.inspector;
    let nodeFront = this.inspector.selection.nodeFront;
    let state = this.store.getState();
    let enabled = !state.boxModel.geometryEditorEnabled;

    this.highlighters.toggleGeometryHighlighter(nodeFront);
    this.store.dispatch(updateGeometryEditorEnabled(enabled));

    if (enabled) {
      // Hide completely the geometry editor if the picker is clicked or a new node front
      toolbox.on("picker-started", this.onHideGeometryEditor);
      selection.on("new-node-front", this.onHideGeometryEditor);
      // Temporary hide the geometry editor
      markup.on("leave", this.onMarkupViewLeave);
      markup.on("node-hover", this.onMarkupViewNodeHover);
    } else {
      toolbox.off("picker-started", this.onHideGeometryEditor);
      selection.off("new-node-front", this.onHideGeometryEditor);
      markup.off("leave", this.onMarkupViewLeave);
      markup.off("node-hover", this.onMarkupViewNodeHover);
    }
  },

};

module.exports = BoxModel;
