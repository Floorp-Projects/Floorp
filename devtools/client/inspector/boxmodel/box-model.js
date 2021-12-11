/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const boxModelReducer = require("devtools/client/inspector/boxmodel/reducers/box-model");
const {
  updateGeometryEditorEnabled,
  updateLayout,
  updateOffsetParent,
} = require("devtools/client/inspector/boxmodel/actions/box-model");

loader.lazyRequireGetter(
  this,
  "EditingSession",
  "devtools/client/inspector/boxmodel/utils/editing-session"
);
loader.lazyRequireGetter(
  this,
  "InplaceEditor",
  "devtools/client/shared/inplace-editor",
  true
);
loader.lazyRequireGetter(
  this,
  "RulePreviewTooltip",
  "devtools/client/shared/widgets/tooltip/RulePreviewTooltip"
);

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

  this.store.injectReducer("boxModel", boxModelReducer);

  this.updateBoxModel = this.updateBoxModel.bind(this);

  this.onHideGeometryEditor = this.onHideGeometryEditor.bind(this);
  this.onMarkupViewLeave = this.onMarkupViewLeave.bind(this);
  this.onMarkupViewNodeHover = this.onMarkupViewNodeHover.bind(this);
  this.onNewSelection = this.onNewSelection.bind(this);
  this.onShowBoxModelEditor = this.onShowBoxModelEditor.bind(this);
  this.onShowRulePreviewTooltip = this.onShowRulePreviewTooltip.bind(this);
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

    if (this._geometryEditorEventsAbortController) {
      this._geometryEditorEventsAbortController.abort();
      this._geometryEditorEventsAbortController = null;
    }

    if (this._tooltip) {
      this._tooltip.destroy();
    }

    this.untrackReflows();

    this.elementRules = null;
    this._highlighters = null;
    this._tooltip = null;
    this.document = null;
    this.inspector = null;
  },

  get highlighters() {
    if (!this._highlighters) {
      // highlighters is a lazy getter in the inspector.
      this._highlighters = this.inspector.highlighters;
    }

    return this._highlighters;
  },

  get rulePreviewTooltip() {
    if (!this._tooltip) {
      this._tooltip = new RulePreviewTooltip(this.inspector.toolbox.doc);
    }

    return this._tooltip;
  },

  /**
   * Returns an object containing the box model's handler functions used in the box
   * model's React component props.
   */
  getComponentProps() {
    return {
      onShowBoxModelEditor: this.onShowBoxModelEditor,
      onShowRulePreviewTooltip: this.onShowRulePreviewTooltip,
      onToggleGeometryEditor: this.onToggleGeometryEditor,
    };
  },

  /**
   * Returns true if the layout panel is visible, and false otherwise.
   */
  isPanelVisible() {
    return (
      this.inspector.toolbox &&
      this.inspector.sidebar &&
      this.inspector.toolbox.currentToolId === "inspector" &&
      this.inspector.sidebar.getCurrentTabID() === "layoutview"
    );
  },

  /**
   * Returns true if the layout panel is visible and the current element is valid to
   * be displayed in the view.
   */
  isPanelVisibleAndNodeValid() {
    return (
      this.isPanelVisible() &&
      this.inspector.selection.isConnected() &&
      this.inspector.selection.isElementNode()
    );
  },

  /**
   * Starts listening to reflows in the current tab.
   */
  trackReflows() {
    this.inspector.on("reflow-in-selected-target", this.updateBoxModel);
  },

  /**
   * Stops listening to reflows in the current tab.
   */
  untrackReflows() {
    this.inspector.off("reflow-in-selected-target", this.updateBoxModel);
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

    const lastRequest = async function() {
      if (
        !this.inspector ||
        !this.isPanelVisible() ||
        !this.inspector.selection.isConnected() ||
        !this.inspector.selection.isElementNode()
      ) {
        return null;
      }

      const { nodeFront } = this.inspector.selection;
      const inspectorFront = this.getCurrentInspectorFront();
      const { pageStyle } = inspectorFront;

      let layout = await pageStyle.getLayout(nodeFront, {
        autoMargins: true,
      });

      const styleEntries = await pageStyle.getApplied(nodeFront, {
        // We don't need styles applied to pseudo elements of the current node.
        skipPseudo: true,
      });
      this.elementRules = styleEntries.map(e => e.rule);

      // Update the layout properties with whether or not the element's position is
      // editable with the geometry editor.
      const isPositionEditable = await pageStyle.isPositionEditable(nodeFront);

      layout = Object.assign({}, layout, {
        isPositionEditable,
      });

      // Update the redux store with the latest offset parent DOM node
      const offsetParent = await inspectorFront.walker.getOffsetParent(
        nodeFront
      );
      this.store.dispatch(updateOffsetParent(offsetParent));

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
    }
      .bind(this)()
      .catch(error => {
        // If we failed because we were being destroyed while waiting for a request, ignore.
        if (this.document) {
          console.error(error);
        }
      });

    this._lastRequest = lastRequest;
  },

  /**
   * Hides the geometry editor and updates the box moodel store with the new
   * geometry editor enabled state.
   */
  onHideGeometryEditor() {
    this.highlighters.hideGeometryEditor();
    this.store.dispatch(updateGeometryEditorEnabled(false));

    if (this._geometryEditorEventsAbortController) {
      this._geometryEditorEventsAbortController.abort();
      this._geometryEditorEventsAbortController = null;
    }
  },

  /**
   * Handler function that re-shows the geometry editor for an element that already
   * had the geometry editor enabled. This handler function is called on a "leave" event
   * on the markup view.
   */
  onMarkupViewLeave() {
    const state = this.store.getState();
    const enabled = state.boxModel.geometryEditorEnabled;

    if (!enabled) {
      return;
    }

    const nodeFront = this.inspector.selection.nodeFront;
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

    if (
      this.inspector.selection.isConnected() &&
      this.inspector.selection.isElementNode()
    ) {
      this.trackReflows();
    }

    this.updateBoxModel("new-selection");
  },

  /**
   * Shows the RulePreviewTooltip when a box model editable value is hovered on the
   * box model panel.
   *
   * @param  {Element} target
   *         The target element.
   * @param  {String} property
   *         The name of the property.
   */
  onShowRulePreviewTooltip(target, property) {
    const { highlightProperty } = this.inspector.getPanel("ruleview").view;
    const isHighlighted = highlightProperty(property);

    // Only show the tooltip if the property is not highlighted.
    // TODO: In the future, use an associated ruleId for toggling the tooltip instead of
    // the Boolean returned from highlightProperty.
    if (!isHighlighted) {
      this.rulePreviewTooltip.show(target);
    }
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
    const session = new EditingSession({
      inspector: this.inspector,
      doc: this.document,
      elementRules: this.elementRules,
    });
    const initialValue = session.getProperty(property);

    const editor = new InplaceEditor(
      {
        element: element,
        initial: initialValue,
        contentType: InplaceEditor.CONTENT_TYPES.CSS_VALUE,
        property: {
          name: property,
        },
        start: self => {
          self.elt.parentNode.classList.add("boxmodel-editing");
        },
        change: value => {
          if (NUMERIC.test(value)) {
            value += "px";
          }

          const properties = [{ name: property, value: value }];

          if (property.substring(0, 7) == "border-") {
            const bprop = property.substring(0, property.length - 5) + "style";
            const style = session.getProperty(bprop);
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
        cssProperties: this.inspector.cssProperties,
      },
      event
    );
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

    if (
      this.inspector.selection.isConnected() &&
      this.inspector.selection.isElementNode()
    ) {
      this.trackReflows();
    }

    this.updateBoxModel();
  },

  /**
   * Toggles on/off the geometry editor for the current element when the geometry editor
   * toggle button is clicked.
   */
  onToggleGeometryEditor() {
    const { markup, selection, toolbox } = this.inspector;
    const nodeFront = this.inspector.selection.nodeFront;
    const state = this.store.getState();
    const enabled = !state.boxModel.geometryEditorEnabled;

    this.highlighters.toggleGeometryHighlighter(nodeFront);
    this.store.dispatch(updateGeometryEditorEnabled(enabled));

    if (enabled) {
      this._geometryEditorEventsAbortController = new AbortController();
      const eventListenersConfig = {
        signal: this._geometryEditorEventsAbortController.signal,
      };
      // Hide completely the geometry editor if:
      // - the picker is clicked
      // - or if a new node is selected
      toolbox.nodePicker.on(
        "picker-started",
        this.onHideGeometryEditor,
        eventListenersConfig
      );
      selection.on(
        "new-node-front",
        this.onHideGeometryEditor,
        eventListenersConfig
      );
      // Temporarily hide the geometry editor
      markup.on("leave", this.onMarkupViewLeave, eventListenersConfig);
      markup.on("node-hover", this.onMarkupViewNodeHover, eventListenersConfig);
    } else if (this._geometryEditorEventsAbortController) {
      this._geometryEditorEventsAbortController.abort();
      this._geometryEditorEventsAbortController = null;
    }
  },

  getCurrentInspectorFront() {
    return this.inspector.selection.nodeFront.inspectorFront;
  },
};

module.exports = BoxModel;
