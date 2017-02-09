/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { Task } = require("devtools/shared/task");
const { getCssProperties } = require("devtools/shared/fronts/css-properties");
const { ReflowFront } = require("devtools/shared/fronts/reflow");

const { InplaceEditor } = require("devtools/client/shared/inplace-editor");
const { createFactory, createElement } = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const {
  updateLayout,
} = require("./actions/box-model");
const {
  updateGridHighlighted,
  updateGrids,
} = require("./actions/grids");
const {
  updateShowGridLineNumbers,
  updateShowInfiniteLines,
} = require("./actions/highlighter-settings");

const App = createFactory(require("./components/App"));
const Store = require("./store");

const EditingSession = require("./utils/editing-session");

const { LocalizationHelper } = require("devtools/shared/l10n");
const INSPECTOR_L10N =
  new LocalizationHelper("devtools/client/locales/inspector.properties");

const NUMERIC = /^-?[\d\.]+$/;
const SHOW_GRID_LINE_NUMBERS = "devtools.gridinspector.showGridLineNumbers";
const SHOW_INFINITE_LINES_PREF = "devtools.gridinspector.showInfiniteLines";

function LayoutView(inspector, window) {
  this.document = window.document;
  this.highlighters = inspector.highlighters;
  this.inspector = inspector;
  this.store = null;
  this.walker = this.inspector.walker;

  this.updateBoxModel = this.updateBoxModel.bind(this);

  this.onGridLayoutChange = this.onGridLayoutChange.bind(this);
  this.onHighlighterChange = this.onHighlighterChange.bind(this);
  this.onNewSelection = this.onNewSelection.bind(this);
  this.onSidebarSelect = this.onSidebarSelect.bind(this);

  this.init();

  this.highlighters.on("grid-highlighter-hidden", this.onHighlighterChange);
  this.highlighters.on("grid-highlighter-shown", this.onHighlighterChange);
  this.inspector.selection.on("new-node-front", this.onNewSelection);
  this.inspector.sidebar.on("select", this.onSidebarSelect);
}

LayoutView.prototype = {

  /**
   * Initializes the layout view by fetching the LayoutFront from the walker, creating
   * the redux store and adding the view into the inspector sidebar.
   */
  init: Task.async(function* () {
    if (!this.inspector) {
      return;
    }

    this.layoutInspector = yield this.inspector.walker.getLayoutInspector();
    let store = this.store = Store();

    this.loadHighlighterSettings();

    let app = App({
      /**
       * Shows the box model properties under the box model if true, otherwise, hidden by
       * default.
       */
      showBoxModelProperties: true,

      /**
       * Hides the box-model highlighter on the currently selected element.
       */
      onHideBoxModelHighlighter: () => {
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
      onShowBoxModelEditor: (element, event, property) => {
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
      onShowBoxModelHighlighter: (options = {}) => {
        let toolbox = this.inspector.toolbox;
        let nodeFront = this.inspector.selection.nodeFront;

        toolbox.highlighterUtils.highlightNodeFront(nodeFront, options);
      },

      /**
       * Handler for a change in the input checkboxes in the GridList component.
       * Toggles on/off the grid highlighter for the provided grid container element.
       *
       * @param  {NodeFront} node
       *         The NodeFront of the grid container element for which the grid
       *         highlighter is toggled on/off for.
       */
      onToggleGridHighlighter: node => {
        let { highlighterSettings } = this.store.getState();
        this.highlighters.toggleGridHighlighter(node, highlighterSettings);
      },

      /**
       * Handler for a change in the show grid line numbers checkbox in the
       * GridDisplaySettings component. TOggles on/off the option to show the grid line
       * numbers in the grid highlighter. Refreshes the shown grid highlighter for the
       * grids currently highlighted.
       *
       * @param  {Boolean} enabled
       *         Whether or not the grid highlighter should show the grid line numbers.
       */
      onToggleShowGridLineNumbers: enabled => {
        this.store.dispatch(updateShowGridLineNumbers(enabled));
        Services.prefs.setBoolPref(SHOW_GRID_LINE_NUMBERS, enabled);

        let { grids, highlighterSettings } = this.store.getState();

        for (let grid of grids) {
          if (grid.highlighted) {
            this.highlighters.showGridHighlighter(grid.nodeFront, highlighterSettings);
          }
        }
      },

      /**
       * Handler for a change in the extend grid lines infinitely checkbox in the
       * GridDisplaySettings component. Toggles on/off the option to extend the grid
       * lines infinitely in the grid highlighter. Refreshes the shown grid highlighter
       * for grids currently highlighted.
       *
       * @param  {Boolean} enabled
       *         Whether or not the grid highlighter should extend grid lines infinitely.
       */
      onToggleShowInfiniteLines: enabled => {
        this.store.dispatch(updateShowInfiniteLines(enabled));
        Services.prefs.setBoolPref(SHOW_INFINITE_LINES_PREF, enabled);

        let { grids, highlighterSettings } = this.store.getState();

        for (let grid of grids) {
          if (grid.highlighted) {
            this.highlighters.showGridHighlighter(grid.nodeFront, highlighterSettings);
          }
        }
      },
    });

    let provider = createElement(Provider, {
      store,
      id: "layoutview",
      title: INSPECTOR_L10N.getStr("inspector.sidebar.layoutViewTitle2"),
      key: "layoutview",
    }, app);

    let defaultTab = Services.prefs.getCharPref("devtools.inspector.activeSidebar");

    this.inspector.addSidebarTab(
      "layoutview",
      INSPECTOR_L10N.getStr("inspector.sidebar.layoutViewTitle2"),
      provider,
      defaultTab == "layoutview"
    );
  }),

  /**
   * Destruction function called when the inspector is destroyed. Removes event listeners
   * and cleans up references.
   */
  destroy() {
    this.highlighters.off("grid-highlighter-hidden", this.onHighlighterChange);
    this.highlighters.off("grid-highlighter-shown", this.onHighlighterChange);
    this.inspector.selection.off("new-node-front", this.onNewSelection);
    this.inspector.sidebar.off("select", this.onSidebarSelect);
    this.layoutInspector.off("grid-layout-changed", this.onGridLayoutChange);

    if (this.reflowFront) {
      this.untrackReflows();
      this.reflowFront.destroy();
      this.reflowFront = null;
    }

    this.document = null;
    this.inspector = null;
    this.layoutInspector = null;
    this.store = null;
    this.walker = null;
  },

  /**
   * Returns true if the layout panel is visible, and false otherwise.
   */
  isPanelVisible() {
    return this.inspector.toolbox.currentToolId === "inspector" &&
           this.inspector.sidebar &&
           this.inspector.sidebar.getCurrentTabID() === "layoutview";
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
   * Load the grid highligher display settings into the store from the stored preferences.
   */
  loadHighlighterSettings() {
    let { dispatch } = this.store;

    let showGridLineNumbers = Services.prefs.getBoolPref(SHOW_GRID_LINE_NUMBERS);
    let showInfinteLines = Services.prefs.getBoolPref(SHOW_INFINITE_LINES_PREF);

    dispatch(updateShowGridLineNumbers(showGridLineNumbers));
    dispatch(updateShowInfiniteLines(showInfinteLines));
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
   * Updates the grid panel by dispatching the new grid data. This is called when the
   * layout view becomes visible or the view needs to be updated with new grid data.
   *
   * @param {Array|null} gridFronts
   *        Optional array of all GridFront in the current page.
   */
  updateGridPanel: Task.async(function* (gridFronts) {
    // Stop refreshing if the inspector or store is already destroyed.
    if (!this.inspector || !this.store) {
      return;
    }

    // Get all the GridFront from the server if no gridFronts were provided.
    if (!gridFronts) {
      gridFronts = yield this.layoutInspector.getAllGrids(this.walker.rootNode);
    }

    let grids = [];
    for (let i = 0; i < gridFronts.length; i++) {
      let grid = gridFronts[i];
      let nodeFront = yield this.walker.getNodeFromActor(grid.actorID, ["containerEl"]);

      grids.push({
        id: i,
        gridFragments: grid.gridFragments,
        highlighted: nodeFront == this.highlighters.gridHighlighterShown,
        nodeFront,
      });
    }

    this.store.dispatch(updateGrids(grids));
  }),

  /**
   * Handler for "grid-layout-changed" events emitted from the LayoutActor.
   *
   * @param  {Array} grids
   *         Array of all GridFront in the current page.
   */
  onGridLayoutChange(grids) {
    if (this.isPanelVisible()) {
      this.updateGridPanel(grids);
    }
  },

  /**
   * Handler for "grid-highlighter-shown" and "grid-highlighter-hidden" events emitted
   * from the HighlightersOverlay. Updates the NodeFront's grid highlighted state.
   *
   * @param  {Event} event
   *         Event that was triggered.
   * @param  {NodeFront} nodeFront
   *         The NodeFront of the grid container element for which the grid highlighter
   *         is shown for.
   */
  onHighlighterChange(event, nodeFront) {
    let highlighted = event === "grid-highlighter-shown";
    this.store.dispatch(updateGridHighlighted(nodeFront, highlighted));
  },

  /**
   * Selection 'new-node-front' event handler.
   */
  onNewSelection: function () {
    if (!this.isPanelVisibleAndNodeValid()) {
      return;
    }

    this.updateBoxModel();
  },

  /**
   * Handler for the inspector sidebar select event. Starts listening for
   * "grid-layout-changed" if the layout panel is visible. Otherwise, stop
   * listening for grid layout changes. Finally, refresh the layout view if
   * it is visible.
   */
  onSidebarSelect() {
    if (!this.isPanelVisible()) {
      this.layoutInspector.off("grid-layout-changed", this.onGridLayoutChange);
      this.untrackReflows();
      return;
    }

    if (this.inspector.selection.isConnected() &&
        this.inspector.selection.isElementNode()) {
      this.trackReflows();
    }

    this.layoutInspector.on("grid-layout-changed", this.onGridLayoutChange);
    this.updateBoxModel();
    this.updateGridPanel();
  },

};

module.exports = LayoutView;
