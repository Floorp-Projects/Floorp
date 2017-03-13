/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { Task } = require("devtools/shared/task");

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

function GridInspector(inspector, window) {
  this.document = window.document;
  this.highlighters = inspector.highlighters;
  this.inspector = inspector;
  this.store = inspector.store;
  this.walker = this.inspector.walker;

  this.getSwatchColorPickerTooltip = this.getSwatchColorPickerTooltip.bind(this);
  this.setSelectedNode = this.setSelectedNode.bind(this);
  this.updateGridPanel = this.updateGridPanel.bind(this);

  this.onGridLayoutChange = this.onGridLayoutChange.bind(this);
  this.onHighlighterChange = this.onHighlighterChange.bind(this);
  this.onMarkupMutation = this.onMarkupMutation.bind(this);
  this.onSetGridOverlayColor = this.onSetGridOverlayColor.bind(this);
  this.onShowBoxModelHighlighterForNode =
    this.onShowBoxModelHighlighterForNode.bind(this);
  this.onShowGridAreaHighlight = this.onShowGridAreaHighlight.bind(this);
  this.onShowGridCellHighlight = this.onShowGridCellHighlight.bind(this);
  this.onSidebarSelect = this.onSidebarSelect.bind(this);
  this.onToggleGridHighlighter = this.onToggleGridHighlighter.bind(this);
  this.onToggleShowGridLineNumbers = this.onToggleShowGridLineNumbers.bind(this);
  this.onToggleShowInfiniteLines = this.onToggleShowInfiniteLines.bind(this);

  this.init();
}

GridInspector.prototype = {

  /**
   * Initializes the grid inspector by fetching the LayoutFront from the walker, loading
   * the highlighter settings and initalizing the SwatchColorPicker instance.
   */
  init: Task.async(function* () {
    if (!this.inspector) {
      return;
    }

    this.layoutInspector = yield this.inspector.walker.getLayoutInspector();

    this.loadHighlighterSettings();

    // Create a shared SwatchColorPicker instance to be reused by all GridItem components.
    this.swatchColorPickerTooltip = new SwatchColorPickerTooltip(
      this.inspector.toolbox.doc,
      this.inspector,
      {
        supportsCssColor4ColorFunction: () => false
      }
    );

    this.highlighters.on("grid-highlighter-hidden", this.onHighlighterChange);
    this.highlighters.on("grid-highlighter-shown", this.onHighlighterChange);
    this.inspector.on("markupmutation", this.onMarkupMutation);
    this.inspector.sidebar.on("select", this.onSidebarSelect);

    this.onSidebarSelect();
  }),

  /**
   * Destruction function called when the inspector is destroyed. Removes event listeners
   * and cleans up references.
   */
  destroy() {
    this.highlighters.off("grid-highlighter-hidden", this.onHighlighterChange);
    this.highlighters.off("grid-highlighter-shown", this.onHighlighterChange);
    this.inspector.off("markupmutation", this.onMarkupMutation);
    this.inspector.sidebar.off("select", this.onSidebarSelect);
    this.layoutInspector.off("grid-layout-changed", this.onGridLayoutChange);

    this.swatchColorPickerTooltip.destroy();

    this.document = null;
    this.highlighters = null;
    this.inspector = null;
    this.layoutInspector = null;
    this.store = null;
    this.swatchColorPickerTooltip = null;
    this.walker = null;
  },

  getComponentProps() {
    return {
      getSwatchColorPickerTooltip: this.getSwatchColorPickerTooltip,
      setSelectedNode: this.setSelectedNode,
      onSetGridOverlayColor: this.onSetGridOverlayColor,
      onShowBoxModelHighlighterForNode: this.onShowBoxModelHighlighterForNode,
      onShowGridAreaHighlight: this.onShowGridAreaHighlight,
      onShowGridCellHighlight: this.onShowGridCellHighlight,
      onToggleGridHighlighter: this.onToggleGridHighlighter,
      onToggleShowGridLineNumbers: this.onToggleShowGridLineNumbers,
      onToggleShowInfiniteLines: this.onToggleShowInfiniteLines,
    };
  },

  /**
   * Returns the initial color linked to a grid container. Will attempt to check the
   * current grid highlighter state and the store.
   *
   * @param  {NodeFront} nodeFront
   *         The NodeFront for which we need the color.
   * @param  {String} fallbackColor
   *         The color to use if no color could be found for the node front.
   * @return {String} color
   *         The color to use.
   */
  getInitialGridColor(nodeFront, fallbackColor) {
    let highlighted = nodeFront == this.highlighters.gridHighlighterShown;

    let color;
    if (highlighted && this.highlighters.state.grid.options) {
      // If the node front is currently highlighted, use the color from the highlighter
      // options.
      color = this.highlighters.state.grid.options.color;
    } else {
      // Otherwise use the color defined in the store for this node front.
      color = this.getGridColorForNodeFront(nodeFront);
    }

    return color || fallbackColor;
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
   * Retrieve the shared SwatchColorPicker instance.
   */
  getSwatchColorPickerTooltip() {
    return this.swatchColorPickerTooltip;
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
   * Set the inspector selection.
   *
   * @param {NodeFront} nodeFront
   *        The NodeFront corresponding to the new selection.
   */
  setSelectedNode(nodeFront) {
    this.inspector.selection.setNodeFront(nodeFront, "layout-panel");
  },

  /**
   * Updates the grid panel by dispatching the new grid data. This is called when the
   * layout view becomes visible or the view needs to be updated with new grid data.
   *
   * @param  {Array|null} gridFronts
   *         Optional array of all GridFront in the current page.
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
      let color = this.getInitialGridColor(nodeFront, fallbackColor);

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
   * @param  {Object} options
   *         The highlighter options used for the highlighter being shown/hidden.
   */
  onHighlighterChange(event, nodeFront, options) {
    let highlighted = event === "grid-highlighter-shown";
    let { color } = options;
    this.store.dispatch(updateGridHighlighted(nodeFront, highlighted));
    this.store.dispatch(updateGridColor(nodeFront, color));
  },

  /**
   * Handler for the "markupmutation" event fired by the inspector. On markup mutations,
   * update the grid panel content.
   */
  onMarkupMutation() {
    this.updateGridPanel();
  },

  /**
   * Handler for a change in the grid overlay color picker for a grid container.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the grid container element for which the grid color is
   *         being updated.
   * @param  {String} color
   *         A hex string representing the color to use.
   */
  onSetGridOverlayColor(node, color) {
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

  /**
   * Shows the box-model highlighter on the element corresponding to the provided
   * NodeFront.
   *
   * @param  {NodeFront} nodeFront
   *         The node to highlight.
   * @param  {Object} options
   *         Options passed to the highlighter actor.
   */
  onShowBoxModelHighlighterForNode(nodeFront, options) {
    let toolbox = this.inspector.toolbox;
    toolbox.highlighterUtils.highlightNodeFront(nodeFront, options);
  },

  /**
   * Highlights the grid area in the CSS Grid Highlighter for the given grid.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the grid container element for which the grid
   *         highlighter is highlighted for.
   * @param  {String} gridAreaName
   *         The name of the grid area for which the grid highlighter
   *         is highlighted for.
   * @param  {String} color
   *         The color of the grid area for which the grid highlighter
   *         is highlighted for.
   */
  onShowGridAreaHighlight(node, gridAreaName, color) {
    let { highlighterSettings } = this.store.getState();

    highlighterSettings.showGridArea = gridAreaName;
    highlighterSettings.color = color;

    this.highlighters.showGridHighlighter(node, highlighterSettings);
  },

  /**
   * Highlights the grid cell in the CSS Grid Highlighter for the given grid.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the grid container element for which the grid
   *         highlighter is highlighted for.
   * @param  {Number|null} gridFragmentIndex
   *         The index of the grid fragment for which the grid highlighter
   *         is highlighted for.
   * @param  {Number|null} rowNumber
   *         The row number of the grid cell for which the grid highlighter
   *         is highlighted for.
   * @param  {Number|null} columnNumber
   *         The column number of the grid cell for which the grid highlighter
   *         is highlighted for.
   */
  onShowGridCellHighlight(node, gridFragmentIndex, rowNumber, columnNumber) {
    let { highlighterSettings } = this.store.getState();
    highlighterSettings.showGridCell = { gridFragmentIndex, rowNumber, columnNumber };

    this.highlighters.showGridHighlighter(node, highlighterSettings);
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

  /**
   * Handler for a change in the input checkboxes in the GridList component.
   * Toggles on/off the grid highlighter for the provided grid container element.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the grid container element for which the grid
   *         highlighter is toggled on/off for.
   */
  onToggleGridHighlighter(node) {
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
  onToggleShowGridLineNumbers(enabled) {
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
  onToggleShowInfiniteLines(enabled) {
    this.store.dispatch(updateShowInfiniteLines(enabled));
    Services.prefs.setBoolPref(SHOW_INFINITE_LINES_PREF, enabled);

    let { grids } = this.store.getState();

    for (let grid of grids) {
      if (grid.highlighted) {
        let highlighterSettings = this.getGridHighlighterSettings(grid.nodeFront);
        this.highlighters.showGridHighlighter(grid.nodeFront, highlighterSettings);
      }
    }
  },

};

module.exports = GridInspector;
