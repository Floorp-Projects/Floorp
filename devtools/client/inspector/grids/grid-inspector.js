/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { throttle } = require("devtools/client/inspector/shared/utils");

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

loader.lazyRequireGetter(this, "compareFragmentsGeometry", "devtools/client/inspector/grids/utils/utils", true);
loader.lazyRequireGetter(this, "parseURL", "devtools/client/shared/source-utils", true);
loader.lazyRequireGetter(this, "asyncStorage", "devtools/shared/async-storage");

const CSS_GRID_COUNT_HISTOGRAM_ID = "DEVTOOLS_NUMBER_OF_CSS_GRIDS_IN_A_PAGE";

const SHOW_GRID_AREAS = "devtools.gridinspector.showGridAreas";
const SHOW_GRID_LINE_NUMBERS = "devtools.gridinspector.showGridLineNumbers";
const SHOW_INFINITE_LINES_PREF = "devtools.gridinspector.showInfiniteLines";

const TELEMETRY_GRID_AREAS_OVERLAY_CHECKED = "devtools.grid.showGridAreasOverlay.checked";
const TELEMETRY_GRID_LINE_NUMBERS_CHECKED = "devtools.grid.showGridLineNumbers.checked";
const TELEMETRY_INFINITE_LINES_CHECKED = "devtools.grid.showInfiniteLines.checked";

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

class GridInspector {
  constructor(inspector, window) {
    this.document = window.document;
    this.inspector = inspector;
    this.store = inspector.store;
    this.telemetry = inspector.telemetry;
    this.walker = this.inspector.walker;

    this.updateGridPanel = this.updateGridPanel.bind(this);

    this.onHighlighterShown = this.onHighlighterShown.bind(this);
    this.onHighlighterHidden = this.onHighlighterHidden.bind(this);
    this.onNavigate = this.onNavigate.bind(this);
    this.onReflow = throttle(this.onReflow, 500, this);
    this.onSetGridOverlayColor = this.onSetGridOverlayColor.bind(this);
    this.onShowGridOutlineHighlight = this.onShowGridOutlineHighlight.bind(this);
    this.onSidebarSelect = this.onSidebarSelect.bind(this);
    this.onToggleGridHighlighter = this.onToggleGridHighlighter.bind(this);
    this.onToggleShowGridAreas = this.onToggleShowGridAreas.bind(this);
    this.onToggleShowGridLineNumbers = this.onToggleShowGridLineNumbers.bind(this);
    this.onToggleShowInfiniteLines = this.onToggleShowInfiniteLines.bind(this);

    this.init();
  }

  get highlighters() {
    if (!this._highlighters) {
      this._highlighters = this.inspector.highlighters;
    }

    return this._highlighters;
  }

  /**
   * Initializes the grid inspector by fetching the LayoutFront from the walker and
   * loading the highlighter settings.
   */
  async init() {
    if (!this.inspector) {
      return;
    }

    try {
      this.layoutInspector = await this.inspector.walker.getLayoutInspector();
    } catch (e) {
      // This call might fail if called asynchrously after the toolbox is finished
      // closing.
      return;
    }

    this.document.addEventListener("mousemove", () => {
      this.highlighters.on("grid-highlighter-hidden", this.onHighlighterHidden);
      this.highlighters.on("grid-highlighter-shown", this.onHighlighterShown);
    }, { once: true });

    this.inspector.sidebar.on("select", this.onSidebarSelect);
    this.inspector.on("new-root", this.onNavigate);

    this.onSidebarSelect();
  }

  /**
   * Destruction function called when the inspector is destroyed. Removes event listeners
   * and cleans up references.
   */
  destroy() {
    if (this._highlighters) {
      this.highlighters.off("grid-highlighter-hidden", this.onHighlighterHidden);
      this.highlighters.off("grid-highlighter-shown", this.onHighlighterShown);
    }

    this.inspector.sidebar.off("select", this.onSidebarSelect);
    this.inspector.off("new-root", this.onNavigate);

    this.inspector.reflowTracker.untrackReflows(this, this.onReflow);

    this._highlighters = null;
    this.document = null;
    this.inspector = null;
    this.layoutInspector = null;
    this.store = null;
    this.walker = null;
  }

  getComponentProps() {
    return {
      onSetGridOverlayColor: this.onSetGridOverlayColor,
      onShowGridOutlineHighlight: this.onShowGridOutlineHighlight,
      onToggleGridHighlighter: this.onToggleGridHighlighter,
      onToggleShowGridAreas: this.onToggleShowGridAreas,
      onToggleShowGridLineNumbers: this.onToggleShowGridLineNumbers,
      onToggleShowInfiniteLines: this.onToggleShowInfiniteLines,
    };
  }

  /**
   * Returns the initial color linked to a grid container. Will attempt to check the
   * current grid highlighter state and the store.
   *
   * @param  {NodeFront} nodeFront
   *         The NodeFront for which we need the color.
   * @param  {String} customColor
   *         The color fetched from the custom palette, if it exists.
   * @param  {String} fallbackColor
   *         The color to use if no color could be found for the node front.
   * @return {String} color
   *         The color to use.
   */
  getInitialGridColor(nodeFront, customColor, fallbackColor) {
    const highlighted = nodeFront == this.highlighters.gridHighlighterShown;

    let color;
    if (customColor) {
      color = customColor;
    } else if (highlighted && this.highlighters.state.grid.options) {
      // If the node front is currently highlighted, use the color from the highlighter
      // options.
      color = this.highlighters.state.grid.options.color;
    } else {
      // Otherwise use the color defined in the store for this node front.
      color = this.getGridColorForNodeFront(nodeFront);
    }

    return color || fallbackColor;
  }

  /**
   * Returns the color set for the grid highlighter associated with the provided
   * nodeFront.
   *
   * @param  {NodeFront} nodeFront
   *         The NodeFront for which we need the color.
   */
  getGridColorForNodeFront(nodeFront) {
    const { grids } = this.store.getState();

    for (const grid of grids) {
      if (grid.nodeFront === nodeFront) {
        return grid.color;
      }
    }

    return null;
  }

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
  }

  /**
   * Returns true if the layout panel is visible, and false otherwise.
   */
  isPanelVisible() {
    return this.inspector && this.inspector.toolbox && this.inspector.sidebar &&
           this.inspector.toolbox.currentToolId === "inspector" &&
           this.inspector.sidebar.getCurrentTabID() === "layoutview";
  }

  /**
   * Updates the grid panel by dispatching the new grid data. This is called when the
   * layout view becomes visible or the view needs to be updated with new grid data.
   */
  async updateGridPanel() {
    // Stop refreshing if the inspector or store is already destroyed.
    if (!this.inspector || !this.store) {
      return;
    }

    // Get all the GridFront from the server if no gridFronts were provided.
    let gridFronts;
    try {
      gridFronts = await this.layoutInspector.getGrids(this.walker.rootNode);
    } catch (e) {
      // This call might fail if called asynchrously after the toolbox is finished
      // closing.
      return;
    }

    if (!gridFronts.length) {
      this.store.dispatch(updateGrids([]));
      this.inspector.emit("grid-panel-updated");
      return;
    }

    const currentUrl = this.inspector.target.url;

    // Log how many CSS Grid elements DevTools sees.
    if (currentUrl != this.inspector.previousURL) {
      this.telemetry.getHistogramById(CSS_GRID_COUNT_HISTOGRAM_ID).add(gridFronts.length);
      this.inspector.previousURL = currentUrl;
    }

    // Get the hostname, if there is no hostname, fall back on protocol
    // ex: `data:` uri, and `about:` pages
    const hostname = parseURL(currentUrl).hostname || parseURL(currentUrl).protocol;
    const customColors = await asyncStorage.getItem("gridInspectorHostColors") || {};

    const grids = [];
    for (let i = 0; i < gridFronts.length; i++) {
      const grid = gridFronts[i];
      let nodeFront = grid.containerNodeFront;

      // If the GridFront didn't yet have access to the NodeFront for its container, then
      // get it from the walker. This happens when the walker hasn't yet seen this
      // particular DOM Node in the tree yet, or when we are connected to an older server.
      if (!nodeFront) {
        try {
          nodeFront = await this.walker.getNodeFromActor(grid.actorID, ["containerEl"]);
        } catch (e) {
          // This call might fail if called asynchrously after the toolbox is finished
          // closing.
          return;
        }
      }

      const colorForHost = customColors[hostname] ? customColors[hostname][i] : null;
      const fallbackColor = GRID_COLORS[i % GRID_COLORS.length];
      const color = this.getInitialGridColor(nodeFront, colorForHost, fallbackColor);
      const highlighted = nodeFront == this.highlighters.gridHighlighterShown;

      grids.push({
        id: i,
        actorID: grid.actorID,
        color,
        direction: grid.direction,
        gridFragments: grid.gridFragments,
        highlighted,
        nodeFront,
        writingMode: grid.writingMode,
      });
    }

    this.store.dispatch(updateGrids(grids));
    this.inspector.emit("grid-panel-updated");
  }
  /**
   * Handler for "grid-highlighter-shown" events emitted from the
   * HighlightersOverlay. Passes nodefront and event name to handleHighlighterChange.
   * Required since on and off events need the same reference object.
   *
   * @param  {NodeFront} nodeFront
   *         The NodeFront of the grid container element for which the grid
   *         highlighter is shown for.
   * @param  {Object} options
   *         The highlighter options used for the highlighter being shown/hidden.
   */
  onHighlighterShown(nodeFront, options) {
    this.onHighlighterChange(nodeFront, true, options);
  }

  /**
   * Handler for "grid-highlighter-hidden" events emitted from the
   * HighlightersOverlay. Passes nodefront and event name to handleHighlighterChange.
   * Required since on and off events need the same reference object.
   *
   * @param  {NodeFront} nodeFront
   *         The NodeFront of the grid container element for which the grid highlighter
   *         is hidden for.
   * @param  {Object} options
   *         The highlighter options used for the highlighter being shown/hidden.
   */
  onHighlighterHidden(nodeFront, options) {
    this.onHighlighterChange(nodeFront, false, options);
  }

  /**
   * Handler for "grid-highlighter-shown" and "grid-highlighter-hidden" events emitted
   * from the HighlightersOverlay. Updates the NodeFront's grid highlighted state.
   *
   * @param  {NodeFront} nodeFront
   *         The NodeFront of the grid container element for which the grid highlighter
   *         is shown for.
   * @param  {Boolean} highlighted
   *         If the grid should be updated to highlight or hide.
   * @param  {Object} options
   *         The highlighter options used for the highlighter being shown/hidden.
   */
  onHighlighterChange(nodeFront, highlighted, options = {}) {
    if (!this.isPanelVisible()) {
      return;
    }

    const { grids } = this.store.getState();
    const grid = grids.find(g => g.nodeFront === nodeFront);

    if (!grid || grid.highlighted === highlighted) {
      return;
    }

    this.store.dispatch(updateGridHighlighted(nodeFront, highlighted));
  }

  /**
   * Handler for "new-root" event fired by the inspector, which indicates a page
   * navigation. Updates grid panel contents.
   */
  onNavigate() {
    if (this.isPanelVisible()) {
      this.updateGridPanel();
    }
  }

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
  async onReflow() {
    if (!this.isPanelVisible()) {
      return;
    }

    // The list of grids currently displayed.
    const { grids } = this.store.getState();

    // The new list of grids from the server.
    let newGridFronts;
    try {
      newGridFronts = await this.layoutInspector.getGrids(this.walker.rootNode);
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
  }

  /**
   * Handler for a change in the grid overlay color picker for a grid container.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the grid container element for which the grid color is
   *         being updated.
   * @param  {String} color
   *         A hex string representing the color to use.
   */
  async onSetGridOverlayColor(node, color) {
    this.store.dispatch(updateGridColor(node, color));

    const { grids } = this.store.getState();
    const currentUrl = this.inspector.target.url;
    // Get the hostname, if there is no hostname, fall back on protocol
    // ex: `data:` uri, and `about:` pages
    const hostname = parseURL(currentUrl).hostname || parseURL(currentUrl).protocol;
    const customGridColors = await asyncStorage.getItem("gridInspectorHostColors") || {};

    for (const grid of grids) {
      if (grid.nodeFront === node) {
        if (!customGridColors[hostname]) {
          customGridColors[hostname] = [];
        }
        // Update the custom color for the grid in this position.
        customGridColors[hostname][grid.id] = color;
        await asyncStorage.setItem("gridInspectorHostColors", customGridColors);

        // If the grid for which the color was updated currently has a highlighter, update
        // the color.
        if (grid.highlighted) {
          this.highlighters.showGridHighlighter(node);
        }
      }
    }
  }

  /**
   * Highlights the grid area and cell in the CSS Grid Highlighter for the given grid
   * container element and selected grid area and cell options.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the grid container element for which the grid highlighter
   *         is highlighted for.
   * @param  {Object} options
   *         The options object has the following properties which corresponds to the
   *         required parameters for showing the grid cell or area highlights.
   *         See css-grid.js.
   *         {
   *           showGridCell: {
   *             gridFragmentIndex: Number,
   *             rowNumber: Number,
   *             columnNumber: Number,
   *           },
   *           showGridArea: String,
   *         }
   */
  onShowGridOutlineHighlight(node, options) {
    this.highlighters.showGridHighlighter(node, options);
  }

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

    this.inspector.reflowTracker.trackReflows(this, this.onReflow);
    this.updateGridPanel();
  }

  /**
   * Handler for a change in the input checkboxes in the GridList component.
   * Toggles on/off the grid highlighter for the provided grid container element.
   *
   * @param  {NodeFront} node
   *         The NodeFront of the grid container element for which the grid
   *         highlighter is toggled on/off for.
   */
  onToggleGridHighlighter(node) {
    const { grids } = this.store.getState();
    const grid = grids.find(g => g.nodeFront === node);
    this.store.dispatch(updateGridHighlighted(node, !grid.highlighted));
    this.highlighters.toggleGridHighlighter(node, "grid");
  }

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
      this.telemetry.scalarSet(TELEMETRY_GRID_AREAS_OVERLAY_CHECKED, 1);
    }

    const { grids } = this.store.getState();

    for (const grid of grids) {
      if (grid.highlighted) {
        this.highlighters.showGridHighlighter(grid.nodeFront);
      }
    }
  }

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
      this.telemetry.scalarSet(TELEMETRY_GRID_LINE_NUMBERS_CHECKED, 1);
    }

    const { grids } = this.store.getState();

    for (const grid of grids) {
      if (grid.highlighted) {
        this.highlighters.showGridHighlighter(grid.nodeFront);
      }
    }
  }

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
      this.telemetry.scalarSet(TELEMETRY_INFINITE_LINES_CHECKED, 1);
    }

    const { grids } = this.store.getState();

    for (const grid of grids) {
      if (grid.highlighted) {
        this.highlighters.showGridHighlighter(grid.nodeFront);
      }
    }
  }
}

module.exports = GridInspector;
