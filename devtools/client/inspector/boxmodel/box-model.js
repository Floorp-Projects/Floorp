/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Task } = require("devtools/shared/task");
const { getCssProperties } = require("devtools/shared/fronts/css-properties");
const { ReflowFront } = require("devtools/shared/fronts/reflow");

const { InplaceEditor } = require("devtools/client/shared/inplace-editor");

const { updateLayout } = require("./actions/box-model");

const EditingSession = require("./utils/editing-session");

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
  this.onNewSelection = this.onNewSelection.bind(this);
  this.onShowBoxModelEditor = this.onShowBoxModelEditor.bind(this);
  this.onShowBoxModelHighlighter = this.onShowBoxModelHighlighter.bind(this);
  this.onSidebarSelect = this.onSidebarSelect.bind(this);

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

    if (this.reflowFront) {
      this.untrackReflows();
      this.reflowFront.destroy();
      this.reflowFront = null;
    }

    this.document = null;
    this.inspector = null;
    this.walker = null;
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
    };
  },

  /**
   * Returns true if the computed or layout panel is visible, and false otherwise.
   */
  isPanelVisible() {
    return this.inspector.toolbox.currentToolId === "inspector" &&
           this.inspector.sidebar &&
           (this.inspector.sidebar.getCurrentTabID() === "layoutview" ||
            this.inspector.sidebar.getCurrentTabID() === "computedview");
  },

  /**
   * Returns true if the layout panel is visible and the current node is valid to
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
    if (!this.reflowFront) {
      let { target } = this.inspector;
      if (target.form.reflowActor) {
        this.reflowFront = ReflowFront(target.client,
                                       target.form);
      } else {
        return;
      }
    }

    this.reflowFront.on("reflows", this.updateBoxModel);
    this.reflowFront.start();
  },

  /**
   * Stops listening to reflows in the current tab.
   */
  untrackReflows() {
    if (!this.reflowFront) {
      return;
    }

    this.reflowFront.off("reflows", this.updateBoxModel);
    this.reflowFront.stop();
  },

  /**
   * Updates the box model panel by dispatching the new layout data.
   */
  updateBoxModel() {
    let lastRequest = Task.spawn((function* () {
      if (!(this.isPanelVisible() &&
          this.inspector.selection.isConnected() &&
          this.inspector.selection.isElementNode())) {
        return null;
      }

      let node = this.inspector.selection.nodeFront;
      let layout = yield this.inspector.pageStyle.getLayout(node, {
        autoMargins: true,
      });
      let styleEntries = yield this.inspector.pageStyle.getApplied(node, {});
      this.elementRules = styleEntries.map(e => e.rule);

      // Update the redux store with the latest layout properties and update the box
      // model view.
      this.store.dispatch(updateLayout(layout));

      // If a subsequent request has been made, wait for that one instead.
      if (this._lastRequest != lastRequest) {
        return this._lastRequest;
      }

      this._lastRequest = null;

      this.inspector.emit("boxmodel-view-updated");
      return null;
    }).bind(this)).catch(console.error);

    this._lastRequest = lastRequest;
  },

  /**
   * Selection 'new-node-front' event handler.
   */
  onNewSelection: function () {
    if (!this.isPanelVisibleAndNodeValid()) {
      return;
    }

    if (this.inspector.selection.isConnected() &&
        this.inspector.selection.isElementNode()) {
      this.trackReflows();
    }

    this.updateBoxModel();
  },

  /**
   * Hides the box-model highlighter on the currently selected element.
   */
  onHideBoxModelHighlighter() {
    let toolbox = this.inspector.toolbox;
    toolbox.highlighterUtils.unhighlight();
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

        session.setProperties(properties).catch(e => console.error(e));
      },
      done: (value, commit) => {
        editor.elt.parentNode.classList.remove("boxmodel-editing");
        if (!commit) {
          session.revert().then(() => {
            session.destroy();
          }, e => console.error(e));
          return;
        }

        let node = this.inspector.selection.nodeFront;
        this.inspector.pageStyle.getLayout(node, {
          autoMargins: true,
        }).then(layout => {
          this.store.dispatch(updateLayout(layout));
        }, e => console.error(e));
      },
      contextMenu: this.inspector.onTextBoxContextMenu,
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
    let toolbox = this.inspector.toolbox;
    let nodeFront = this.inspector.selection.nodeFront;

    toolbox.highlighterUtils.highlightNodeFront(nodeFront, options);
  },

  /**
   * Handler for the inspector sidebar select event. Starts listening for
   * "grid-layout-changed" if the layout panel is visible. Otherwise, stop
   * listening for grid layout changes. Finally, refresh the layout view if
   * it is visible.
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

};

module.exports = BoxModel;
