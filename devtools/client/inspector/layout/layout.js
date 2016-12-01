/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { Task } = require("devtools/shared/task");
const { createFactory, createElement } = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

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

const { LocalizationHelper } = require("devtools/shared/l10n");
const INSPECTOR_L10N =
  new LocalizationHelper("devtools/client/locales/inspector.properties");

const SHOW_GRID_LINE_NUMBERS = "devtools.gridinspector.showGridLineNumbers";
const SHOW_INFINITE_LINES_PREF = "devtools.gridinspector.showInfiniteLines";

function LayoutView(inspector, window) {
  this.document = window.document;
  this.highlighters = inspector.highlighters;
  this.inspector = inspector;
  this.store = null;
  this.walker = this.inspector.walker;

  this.onGridLayoutChange = this.onGridLayoutChange.bind(this);
  this.onHighlighterChange = this.onHighlighterChange.bind(this);
  this.onSidebarSelect = this.onSidebarSelect.bind(this);

  this.highlighters.on("grid-highlighter-hidden", this.onHighlighterChange);
  this.highlighters.on("grid-highlighter-shown", this.onHighlighterChange);
  this.inspector.sidebar.on("select", this.onSidebarSelect);

  this.init();
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
    this.inspector.sidebar.off("select", this.onSidebarSelect);
    this.layoutInspector.off("grid-layout-changed", this.onGridLayoutChange);

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
   * Refreshes the layout view by dispatching the new grid data. This is called when the
   * layout view becomes visible or the view needs to be updated with new grid data.
   *
   * @param {Array|null} gridFronts
   *        Optional array of all GridFront in the current page.
   */
  refresh: Task.async(function* (gridFronts) {
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
      this.refresh(grids);
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
   * Handler for the inspector sidebar select event. Starts listening for
   * "grid-layout-changed" if the layout panel is visible. Otherwise, stop
   * listening for grid layout changes. Finally, refresh the layout view if
   * it is visible.
   */
  onSidebarSelect() {
    if (!this.isPanelVisible()) {
      this.layoutInspector.off("grid-layout-changed", this.onGridLayoutChange);
      return;
    }

    this.layoutInspector.on("grid-layout-changed", this.onGridLayoutChange);
    this.refresh();
  },

};

exports.LayoutView = LayoutView;
