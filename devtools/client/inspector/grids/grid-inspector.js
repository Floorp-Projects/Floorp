/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { Task } = require("devtools/shared/task");

const SwatchColorPickerTooltip = require("devtools/client/shared/widgets/tooltip/SwatchColorPickerTooltip");
const { throttle } = require("devtools/client/inspector/shared/utils");
const { compareFragmentsGeometry } = require("devtools/client/inspector/grids/utils/utils");

const {
  updateGridColor,
  updateGridHighlighted,
  updateGrids,
} = require("./actions/grids");
const {
  updateShowGridAreas,
  updateShowGridLineNumbers,
  updateShowInfiniteLines,
} = require("./actions/highlighter-settings");

const CSS_GRID_COUNT_HISTOGRAM_ID = "DEVTOOLS_NUMBER_OF_CSS_GRIDS_IN_A_PAGE";

const SHOW_GRID_AREAS = "devtools.gridinspector.showGridAreas";
const SHOW_GRID_LINE_NUMBERS = "devtools.gridinspector.showGridLineNumbers";
const SHOW_INFINITE_LINES_PREF = "devtools.gridinspector.showInfiniteLines";
// @remove after release 56 (See Bug 1355747)
const PROMOTE_COUNT_PREF = "devtools.promote.layoutview";

// Default grid colors.
const GRID_COLORS = [
  "#9400FF",
  "#DF00A9",
  "#0A84FF",
  "#12BC00",
  "#EA8000",
  "#00B0BD",
  "#D70022",
  "#4B42FF",
  "#B5007F",
  "#058B00",
  "#A47F00",
  "#005A71"
];

function GridInspector(inspector, window) {
  this.document = window.document;
  this.highlighters = inspector.highlighters;
  this.inspector = inspector;
  this.store = inspector.store;
  this.telemetry = inspector.telemetry;
  this.walker = this.inspector.walker;

  this.getSwatchColorPickerTooltip = this.getSwatchColorPickerTooltip.bind(this);
  this.updateGridPanel = this.updateGridPanel.bind(this);

  this.onNavigate = this.onNavigate.bind(this);
  this.onHighlighterChange = this.onHighlighterChange.bind(this);
  this.onReflow = throttle(this.onReflow, 500, this);
  this.onSetGridOverlayColor = this.onSetGridOverlayColor.bind(this);
  this.onShowGridAreaHighlight = this.onShowGridAreaHighlight.bind(this);
  this.onShowGridCellHighlight = this.onShowGridCellHighlight.bind(this);
  this.onShowGridLineNamesHighlight = this.onShowGridLineNamesHighlight.bind(this);
  this.onSidebarSelect = this.onSidebarSelect.bind(this);
  this.onToggleGridHighlighter = this.onToggleGridHighlighter.bind(this);
  this.onToggleShowGridAreas = this.onToggleShowGridAreas.bind(this);
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
    this.inspector.sidebar.on("select", this.onSidebarSelect);
    this.inspector.on("new-root", this.onNavigate);

    this.onSidebarSelect();
  }),

  /**
   * Destruction function called when the inspector is destroyed. Removes event listeners
   * and cleans up references.
   */
  destroy() {
    this.highlighters.off("grid-highlighter-hidden", this.onHighlighterChange);
    this.highlighters.off("grid-highlighter-shown", this.onHighlighterChange);
    this.inspector.sidebar.off("select", this.onSidebarSelect);
    this.inspector.off("new-root", this.onNavigate);

    this.inspector.reflowTracker.untrackReflows(this, this.onReflow);

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
      onSetGridOverlayColor: this.onSetGridOverlayColor,
      onShowGridAreaHighlight: this.onShowGridAreaHighlight,
      onShowGridCellHighlight: this.onShowGridCellHighlight,
      onShowGridLineNamesHighlight: this.onShowGridLineNamesHighlight,
      onToggleGridHighlighter: this.onToggleGridHighlighter,
      onToggleShowGridAreas: this.onToggleShowGridAreas,
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
    return this.inspector && this.inspector.toolbox && this.inspector.sidebar &&
           this.inspector.toolbox.currentToolId === "inspector" &&
           this.inspector.sidebar.getCurrentTabID() === "layoutview";
  },

  /**
   * Load the grid highligher display settings into the store from the stored preferences.
   */
  loadHighlighterSettings() {
    let { dispatch } = this.store;

    let showGridAreas = Services.prefs.getBoolPref(SHOW_GRID_AREAS);
    let showGridLineNumbers = Services.prefs.getBoolPref(SHOW_GRID_LINE_NUMBERS);
    let showInfinteLines = Services.prefs.getBoolPref(SHOW_INFINITE_LINES_PREF);

    dispatch(updateShowGridAreas(showGridAreas));
    dispatch(updateShowGridLineNumbers(showGridLineNumbers));
    dispatch(updateShowInfiniteLines(showInfinteLines));
  },

  showGridHighlighter(node, settings) {
    this.lastHighlighterColor = settings.color;
    this.lastHighlighterNode = node;
    this.lastHighlighterState = true;

    this.highlighters.showGridHighlighter(node, settings);
  },

  toggleGridHighlighter(node, settings) {
    this.lastHighlighterColor = settings.color;
    this.lastHighlighterNode = node;
    this.lastHighlighterState = node !== this.highlighters.gridHighlighterShown;

    this.highlighters.toggleGridHighlighter(node, settings, "grid");
  },

  /**
   * Updates the grid panel by dispatching the new grid data. This is called when the
   * layout view becomes visible or the view needs to be updated with new grid data.
   */
  updateGridPanel: Task.async(function* () {
    // Stop refreshing if the inspector or store is already destroyed.
    if (!this.inspector || !this.store) {
      return;
    }

    // Get all the GridFront from the server if no gridFronts were provided.
    let gridFronts;
    try {
      gridFronts = yield this.layoutInspector.getAllGrids(this.walker.rootNode);
    } catch (e) {
      // This call might fail if called asynchrously after the toolbox is finished
      // closing.
      return;
    }

    // Log how many CSS Grid elements DevTools sees.
    if (gridFronts.length > 0 &&
        this.inspector.target.url != this.inspector.previousURL) {
      this.telemetry.log(CSS_GRID_COUNT_HISTOGRAM_ID, gridFronts.length);
      this.inspector.previousURL = this.inspector.target.url;
    }

    let grids = [];
    for (let i = 0; i < gridFronts.length; i++) {
      let grid = gridFronts[i];

      let nodeFront = grid.containerNodeFront;

      // If the GridFront didn't yet have access to the NodeFront for its container, then
      // get it from the walker. This happens when the walker hasn't yet seen this
      // particular DOM Node in the tree yet, or when we are connected to an older server.
      if (!nodeFront) {
        try {
          nodeFront = yield this.walker.getNodeFromActor(grid.actorID, ["containerEl"]);
        } catch (e) {
          // This call might fail if called asynchrously after the toolbox is finished
          // closing.
          return;
        }
      }

      let fallbackColor = GRID_COLORS[i % GRID_COLORS.length];
      let color = this.getInitialGridColor(nodeFront, fallbackColor);

      grids.push({
        id: i,
        actorID: grid.actorID,
        color,
        gridFragments: grid.gridFragments,
        highlighted: nodeFront == this.highlighters.gridHighlighterShown,
        nodeFront,
      });
    }

    this.store.dispatch(updateGrids(grids));
  }),

  /**
   * Handler for "new-root" event fired by the inspector, which indicates a page
   * navigation. Updates grid panel contents.
   */
  onNavigate() {
    if (this.isPanelVisible()) {
      this.updateGridPanel();
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
  onHighlighterChange(event, nodeFront, options = {}) {
    let highlighted = event === "grid-highlighter-shown";
    let { color } = options;

    // Only tell the store that the highlighter changed if it did change.
    // If we're still highlighting the same node, with the same color, no need to force
    // a refresh.
    if (this.lastHighlighterState !== highlighted ||
        this.lastHighlighterNode !== nodeFront) {
      this.store.dispatch(updateGridHighlighted(nodeFront, highlighted));
    }

    if (this.lastHighlighterColor !== color || this.lastHighlighterNode !== nodeFront) {
      this.store.dispatch(updateGridColor(nodeFront, color));
    }

    this.lastHighlighterColor = null;
    this.lastHighlighterNode = null;
    this.lastHighlighterState = null;
  },

  /**
   * Given a list of new grid fronts, and if we have a currently highlighted grid, check
   * if its fragments have changed.
   *
   * @param  {Array} newGridFronts
   *         A list of GridFront objects.
   * @return {Boolean}
   */
  haveCurrentFragmentsChanged(newGridFronts) {
    const currentNode = this.highlighters.gridHighlighterShown;
    if (!currentNode) {
      return false;
    }

    const newGridFront = newGridFronts.find(g => g.containerNodeFront === currentNode);
    if (!newGridFront) {
      return false;
    }

    const { grids } = this.store.getState();
    const oldFragments = grids.find(g => g.nodeFront === currentNode).gridFragments;
    const newFragments = newGridFront.gridFragments;

    return !compareFragmentsGeometry(oldFragments, newFragments);
  },

  /**
   * Handler for the "reflow" event fired by the inspector's reflow tracker. On reflows,
   * update the grid panel content, because the shape or number of grids on the page may
   * have changed.
   *
   * Note that there may be frequent reflows on the page and that not all of them actually
   * cause the grids to change. So, we want to limit how many times we update the grid
   * panel to only reflows that actually either change the list of grids, or those that
   * change the current outlined grid.
   * To achieve this, this function compares the list of grid containers from before and
   * after the reflow, as well as the grid fragment data on the currently highlighted
   * grid.
   */
  onReflow: Task.async(function* () {
    if (!this.isPanelVisible()) {
      return;
    }

    // The list of grids currently displayed.
    const { grids } = this.store.getState();

    // The new list of grids from the server.
    let newGridFronts;
    try {
      newGridFronts = yield this.layoutInspector.getAllGrids(this.walker.rootNode);
    } catch (e) {
      // This call might fail if called asynchrously after the toolbox is finished
      // closing.
      return;
    }

    // Get the node front(s) from the current grid(s) so we can compare them to them to
    // node(s) of the new grids.
    const oldNodeFronts = grids.map(grid => grid.nodeFront.actorID);

    // In some cases, the nodes for current grids may have been removed from the DOM in
    // which case we need to update.
    if (grids.length && grids.some(grid => !grid.nodeFront.actorID)) {
      this.updateGridPanel(newGridFronts);
      return;
    }

    // Otherwise, continue comparing with the new grids.
    const newNodeFronts = newGridFronts.filter(grid => grid.containerNodeFront)
                                       .map(grid => grid.containerNodeFront.actorID);
    if (grids.length === newGridFronts.length &&
        oldNodeFronts.sort().join(",") == newNodeFronts.sort().join(",")) {
      // Same list of containers, but let's check if the geometry of the current grid has
      // changed, if it hasn't we can safely abort.
      if (!this.highlighters.gridHighlighterShown ||
          (this.highlighters.gridHighlighterShown &&
           !this.haveCurrentFragmentsChanged(newGridFronts))) {
        return;
      }
    }

    // Either the list of containers or the current fragments have changed, do update.
    this.updateGridPanel(newGridFronts);
  }),

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
        this.showGridHighlighter(node, highlighterSettings);
      }
    }
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

    this.showGridHighlighter(node, highlighterSettings);

    this.store.dispatch(updateGridHighlighted(node, true));
  },

  /**
   * Highlights the grid cell in the CSS Grid Highlighter for the given grid.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the grid container element for which the grid
   *         highlighter is highlighted for.
   * @param  {String} color
   *         The color of the grid cell for which the grid highlighter
   *         is highlighted for.
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
  onShowGridCellHighlight(node, color, gridFragmentIndex, rowNumber, columnNumber) {
    let { highlighterSettings } = this.store.getState();

    highlighterSettings.showGridCell = { gridFragmentIndex, rowNumber, columnNumber };
    highlighterSettings.color = color;

    this.showGridHighlighter(node, highlighterSettings);

    this.store.dispatch(updateGridHighlighted(node, true));
  },

  /**
   * Highlights the grid line in the CSS Grid Highlighter for the given grid.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the grid container element for which the grid
   *         highlighter is highlighted for.
   * @param  {Number|null} gridFragmentIndex
   *         The index of the grid fragment for which the grid highlighter
   *         is highlighted for.
   * @param  {String} color
   *         The color of the grid line for which the grid highlighter
   *         is highlighted for.
   * @param  {Number|null} lineNumber
   *         The line number of the grid for which the grid highlighter
   *         is highlighted for.
   * @param  {String|null} type
   *         The type of line for which the grid line is being highlighted for.
   */
  onShowGridLineNamesHighlight(node, gridFragmentIndex, color, lineNumber, type) {
    let { highlighterSettings } = this.store.getState();

    highlighterSettings.showGridLineNames = {
      gridFragmentIndex,
      lineNumber,
      type
    };
    highlighterSettings.color = color;

    this.showGridHighlighter(node, highlighterSettings);

    this.store.dispatch(updateGridHighlighted(node, true));
  },

  /**
   * Handler for the inspector sidebar "select" event. Starts tracking reflows
   * if the layout panel is visible. Otherwise, stop tracking reflows.
   * Finally, refresh the layout view if it is visible.
   */
  onSidebarSelect() {
    if (!this.isPanelVisible()) {
      this.inspector.reflowTracker.untrackReflows(this, this.onReflow);
      return;
    }

    // @remove after release 56 (See Bug 1355747)
    Services.prefs.setIntPref(PROMOTE_COUNT_PREF, 0);

    this.inspector.reflowTracker.trackReflows(this, this.onReflow);
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
    this.toggleGridHighlighter(node, highlighterSettings);

    this.store.dispatch(updateGridHighlighted(node,
      node !== this.highlighters.gridHighlighterShown));
  },

  /**
    * Handler for a change in the show grid areas checkbox in the GridDisplaySettings
    * component. Toggles on/off the option to show the grid areas in the grid highlighter.
    * Refreshes the shown grid highlighter for the grids currently highlighted.
    *
    * @param  {Boolean} enabled
    *         Whether or not the grid highlighter should show the grid areas.
    */
  onToggleShowGridAreas(enabled) {
    this.store.dispatch(updateShowGridAreas(enabled));
    Services.prefs.setBoolPref(SHOW_GRID_AREAS, enabled);

    if (enabled) {
      this.telemetry.toolOpened("gridInspectorShowGridAreasOverlayChecked");
    }

    let { grids } = this.store.getState();

    for (let grid of grids) {
      if (grid.highlighted) {
        let highlighterSettings = this.getGridHighlighterSettings(grid.nodeFront);
        this.highlighters.showGridHighlighter(grid.nodeFront, highlighterSettings);
      }
    }
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

    if (enabled) {
      this.telemetry.toolOpened("gridInspectorShowGridLineNumbersChecked");
    }

    let { grids } = this.store.getState();

    for (let grid of grids) {
      if (grid.highlighted) {
        let highlighterSettings = this.getGridHighlighterSettings(grid.nodeFront);
        this.showGridHighlighter(grid.nodeFront, highlighterSettings);
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

    if (enabled) {
      this.telemetry.toolOpened("gridInspectorShowInfiniteLinesChecked");
    }

    let { grids } = this.store.getState();

    for (let grid of grids) {
      if (grid.highlighted) {
        let highlighterSettings = this.getGridHighlighterSettings(grid.nodeFront);
        this.showGridHighlighter(grid.nodeFront, highlighterSettings);
      }
    }
  },

};

module.exports = GridInspector;
