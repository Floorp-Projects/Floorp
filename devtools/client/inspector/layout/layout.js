/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { Task } = require("devtools/shared/task");

const { createFactory, createElement } = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const SwatchColorPickerTooltip = require("devtools/client/shared/widgets/tooltip/SwatchColorPickerTooltip");

const {
  updateGridColor,
  updateGridHighlighted,
  updateGrids,
} = require("./actions/grids");
const {
  updateShowGridLineNumbers,
  updateShowInfiniteLines,
} = require("./actions/highlighter-settings");

const App = createFactory(require("./components/App"));

const { LocalizationHelper } = require("devtools/shared/l10n");
const INSPECTOR_L10N =
  new LocalizationHelper("devtools/client/locales/inspector.properties");

const SHOW_GRID_LINE_NUMBERS = "devtools.gridinspector.showGridLineNumbers";
const SHOW_INFINITE_LINES_PREF = "devtools.gridinspector.showInfiniteLines";

// Default grid colors.
const GRID_COLORS = [
  "#05E4EE",
  "#BB9DFF",
  "#FFB53B",
  "#71F362",
  "#FF90FF",
  "#FF90FF",
  "#1B80FF",
  "#FF2647"
];

function LayoutView(inspector, window) {
  this.document = window.document;
  this.highlighters = inspector.highlighters;
  this.inspector = inspector;
  this.store = inspector.store;
  this.walker = this.inspector.walker;

  this.onGridLayoutChange = this.onGridLayoutChange.bind(this);
  this.onHighlighterChange = this.onHighlighterChange.bind(this);
  this.onSidebarSelect = this.onSidebarSelect.bind(this);

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

    let {
      onHideBoxModelHighlighter,
      onShowBoxModelEditor,
      onShowBoxModelHighlighter,
    } = this.inspector.boxmodel.getComponentProps();

    this.layoutInspector = yield this.inspector.walker.getLayoutInspector();

    this.loadHighlighterSettings();

    this.highlighters.on("grid-highlighter-hidden", this.onHighlighterChange);
    this.highlighters.on("grid-highlighter-shown", this.onHighlighterChange);
    this.inspector.sidebar.on("select", this.onSidebarSelect);

    // Create a shared SwatchColorPicker instance to be reused by all GridItem components.
    this.swatchColorPickerTooltip = new SwatchColorPickerTooltip(
      this.inspector.toolbox.doc,
      this.inspector,
      {
        supportsCssColor4ColorFunction: () => false
      }
    );

    let app = App({
      /**
       * Retrieve the shared SwatchColorPicker instance.
       */
      getSwatchColorPickerTooltip: () => {
        return this.swatchColorPickerTooltip;
      },

      /**
       * Set the inspector selection.
       * @param {NodeFront} nodeFront
       *        The NodeFront corresponding to the new selection.
       */
      setSelectedNode: (nodeFront) => {
        this.inspector.selection.setNodeFront(nodeFront, "layout-panel");
      },

      /**
       * Shows the box model properties under the box model if true, otherwise, hidden by
       * default.
       */
      showBoxModelProperties: true,

      onHideBoxModelHighlighter,

      /**
       * Handler for a change in the grid overlay color picker for a grid container.
       *
       * @param  {NodeFront} node
       *         The NodeFront of the grid container element for which the grid color is
       *         being updated.
       * @param  {String} color
       *         A hex string representing the color to use.
       */
      onSetGridOverlayColor: (node, color) => {
        this.store.dispatch(updateGridColor(node, color));
        let { grids } = this.store.getState();

        // If the grid for which the color was updated currently has a highlighter, update
        // the color.
        for (let grid of grids) {
          if (grid.nodeFront === node && grid.highlighted) {
            let highlighterSettings = this.getGridHighlighterSettings(node);
            this.highlighters.showGridHighlighter(node, highlighterSettings);
          }
        }
      },

      onShowBoxModelEditor,
      onShowBoxModelHighlighter,

     /**
       * Shows the box-model highlighter on the element corresponding to the provided
       * NodeFront.
       *
       * @param  {NodeFront} nodeFront
       *         The node to highlight.
       * @param  {Object} options
       *         Options passed to the highlighter actor.
       */
      onShowBoxModelHighlighterForNode: (nodeFront, options) => {
        let toolbox = this.inspector.toolbox;
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
        let highlighterSettings = this.getGridHighlighterSettings(node);
        this.highlighters.toggleGridHighlighter(node, highlighterSettings);
      },

      /**
       * Handler for a change in the show grid line numbers checkbox in the
       * GridDisplaySettings component. Toggles on/off the option to show the grid line
       * numbers in the grid highlighter. Refreshes the shown grid highlighter for the
       * grids currently highlighted.
       *
       * @param  {Boolean} enabled
       *         Whether or not the grid highlighter should show the grid line numbers.
       */
      onToggleShowGridLineNumbers: enabled => {
        this.store.dispatch(updateShowGridLineNumbers(enabled));
        Services.prefs.setBoolPref(SHOW_GRID_LINE_NUMBERS, enabled);

        let { grids } = this.store.getState();

        for (let grid of grids) {
          if (grid.highlighted) {
            let highlighterSettings = this.getGridHighlighterSettings(grid.nodeFront);
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

        let { grids } = this.store.getState();

        for (let grid of grids) {
          if (grid.highlighted) {
            let highlighterSettings = this.getGridHighlighterSettings(grid.nodeFront);
            this.highlighters.showGridHighlighter(grid.nodeFront, highlighterSettings);
          }
        }
      }
    });

    let provider = createElement(Provider, {
      store: this.store,
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
   * Returns the color set for the grid highlighter associated with the provided
   * nodeFront.
   *
   * @param  {NodeFront} nodeFront
   *         The NodeFront for which we need the color.
   */
  getGridColorForNodeFront(nodeFront) {
    let { grids } = this.store.getState();

    for (let grid of grids) {
      if (grid.nodeFront === nodeFront) {
        return grid.color;
      }
    }

    return null;
  },

  /**
   * Create a highlighter settings object for the provided nodeFront.
   *
   * @param  {NodeFront} nodeFront
   *         The NodeFront for which we need highlighter settings.
   */
  getGridHighlighterSettings(nodeFront) {
    let { highlighterSettings } = this.store.getState();

    // Get the grid color for the provided nodeFront.
    let color = this.getGridColorForNodeFront(nodeFront);

    // Merge the grid color to the generic highlighter settings.
    return Object.assign({}, highlighterSettings, {
      color
    });
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

      let fallbackColor = GRID_COLORS[i % GRID_COLORS.length];
      let color = this.getGridColorForNodeFront(nodeFront) || fallbackColor;

      grids.push({
        id: i,
        color,
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
    this.updateGridPanel();
  },

};

module.exports = LayoutView;
