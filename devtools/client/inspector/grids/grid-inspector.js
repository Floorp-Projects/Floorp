/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const flags = require("devtools/shared/flags");
const { throttle } = require("devtools/shared/throttle");

const gridsReducer = require("devtools/client/inspector/grids/reducers/grids");
const highlighterSettingsReducer = require("devtools/client/inspector/grids/reducers/highlighter-settings");
const {
  updateGridColor,
  updateGridHighlighted,
  updateGrids,
} = require("devtools/client/inspector/grids/actions/grids");
const {
  updateShowGridAreas,
  updateShowGridLineNumbers,
  updateShowInfiniteLines,
} = require("devtools/client/inspector/grids/actions/highlighter-settings");

loader.lazyRequireGetter(
  this,
  "compareFragmentsGeometry",
  "devtools/client/inspector/grids/utils/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "parseURL",
  "devtools/client/shared/source-utils",
  true
);
loader.lazyRequireGetter(this, "asyncStorage", "devtools/shared/async-storage");

const CSS_GRID_COUNT_HISTOGRAM_ID = "DEVTOOLS_NUMBER_OF_CSS_GRIDS_IN_A_PAGE";

const SHOW_GRID_AREAS = "devtools.gridinspector.showGridAreas";
const SHOW_GRID_LINE_NUMBERS = "devtools.gridinspector.showGridLineNumbers";
const SHOW_INFINITE_LINES_PREF = "devtools.gridinspector.showInfiniteLines";

const TELEMETRY_GRID_AREAS_OVERLAY_CHECKED =
  "devtools.grid.showGridAreasOverlay.checked";
const TELEMETRY_GRID_LINE_NUMBERS_CHECKED =
  "devtools.grid.showGridLineNumbers.checked";
const TELEMETRY_INFINITE_LINES_CHECKED =
  "devtools.grid.showInfiniteLines.checked";

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
  "#005A71",
];

class GridInspector {
  constructor(inspector, window) {
    this.document = window.document;
    this.inspector = inspector;
    this.store = inspector.store;
    this.telemetry = inspector.telemetry;

    // Maximum number of grid highlighters that can be displayed.
    this.maxHighlighters = Services.prefs.getIntPref(
      "devtools.gridinspector.maxHighlighters"
    );

    this.store.injectReducer("grids", gridsReducer);
    this.store.injectReducer("highlighterSettings", highlighterSettingsReducer);

    this.onHighlighterShown = this.onHighlighterShown.bind(this);
    this.onHighlighterHidden = this.onHighlighterHidden.bind(this);
    this.onNavigate = this.onNavigate.bind(this);
    this.onReflow = throttle(this.onReflow, 500, this);
    this.onSetGridOverlayColor = this.onSetGridOverlayColor.bind(this);
    this.onShowGridOutlineHighlight = this.onShowGridOutlineHighlight.bind(
      this
    );
    this.onSidebarSelect = this.onSidebarSelect.bind(this);
    this.onToggleGridHighlighter = this.onToggleGridHighlighter.bind(this);
    this.onToggleShowGridAreas = this.onToggleShowGridAreas.bind(this);
    this.onToggleShowGridLineNumbers = this.onToggleShowGridLineNumbers.bind(
      this
    );
    this.onToggleShowInfiniteLines = this.onToggleShowInfiniteLines.bind(this);
    this.updateGridPanel = this.updateGridPanel.bind(this);

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
      // TODO: Call this again whenever targets are added or removed.
      this.layoutFronts = await this.getLayoutFronts();
    } catch (e) {
      // This call might fail if called asynchrously after the toolbox is finished
      // closing.
      return;
    }

    if (flags.testing) {
      // In tests, we start listening immediately to avoid having to simulate a mousemove.
      this.highlighters.on("grid-highlighter-hidden", this.onHighlighterHidden);
      this.highlighters.on("grid-highlighter-shown", this.onHighlighterShown);
    } else {
      this.document.addEventListener(
        "mousemove",
        () => {
          this.highlighters.on(
            "grid-highlighter-hidden",
            this.onHighlighterHidden
          );
          this.highlighters.on(
            "grid-highlighter-shown",
            this.onHighlighterShown
          );
        },
        { once: true }
      );
    }

    this.inspector.sidebar.on("select", this.onSidebarSelect);
    this.inspector.on("new-root", this.onNavigate);

    this.onSidebarSelect();
  }

  /**
   * Get the LayoutActor fronts for all interesting targets where we have inspectors.
   *
   * @return {Array} The list of LayoutActor fronts
   */
  async getLayoutFronts() {
    const inspectorFronts = await this.inspector.getAllInspectorFronts();

    const layoutFronts = [];
    for (const { walker } of inspectorFronts) {
      const layoutFront = await walker.getLayoutInspector();
      layoutFronts.push(layoutFront);
    }

    return layoutFronts;
  }

  /**
   * Destruction function called when the inspector is destroyed. Removes event listeners
   * and cleans up references.
   */
  destroy() {
    if (this._highlighters) {
      this.highlighters.off(
        "grid-highlighter-hidden",
        this.onHighlighterHidden
      );
      this.highlighters.off("grid-highlighter-shown", this.onHighlighterShown);
    }

    this.inspector.sidebar.off("select", this.onSidebarSelect);
    this.inspector.off("new-root", this.onNavigate);

    this.inspector.off("reflow-in-selected-target", this.onReflow);

    this._highlighters = null;
    this.document = null;
    this.inspector = null;
    this.layoutFronts = null;
    this.store = null;
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
    const highlighted = this.highlighters.gridHighlighters.has(nodeFront);

    let color;
    if (customColor) {
      color = customColor;
    } else if (
      highlighted &&
      this.highlighters.state.grids.has(nodeFront.actorID)
    ) {
      // If the node front is currently highlighted, use the color from the highlighter
      // options.
      color = this.highlighters.state.grids.get(nodeFront.actorID).options
        .color;
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
   * Given a list of new grid fronts, and if there are highlighted grids, check
   * if their fragments have changed.
   *
   * @param  {Array} newGridFronts
   *         A list of GridFront objects.
   * @return {Boolean}
   */
  haveCurrentFragmentsChanged(newGridFronts) {
    const gridHighlighters = this.highlighters.gridHighlighters;

    if (!gridHighlighters.size) {
      return false;
    }

    const gridFronts = newGridFronts.filter(g =>
      gridHighlighters.has(g.containerNodeFront)
    );
    if (!gridFronts.length) {
      return false;
    }

    const { grids } = this.store.getState();

    for (const node of gridHighlighters.keys()) {
      const oldFragments = grids.find(g => g.nodeFront === node).gridFragments;
      const newFragments = newGridFronts.find(
        g => g.containerNodeFront === node
      ).gridFragments;

      if (!compareFragmentsGeometry(oldFragments, newFragments)) {
        return true;
      }
    }

    return false;
  }

  /**
   * Returns true if the layout panel is visible, and false otherwise.
   */
  isPanelVisible() {
    return (
      this.inspector &&
      this.inspector.toolbox &&
      this.inspector.sidebar &&
      this.inspector.toolbox.currentToolId === "inspector" &&
      this.inspector.sidebar.getCurrentTabID() === "layoutview"
    );
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

    try {
      await this._updateGridPanel();
    } catch (e) {
      this._throwUnlessDestroyed(
        e,
        "Inspector destroyed while executing updateGridPanel"
      );
    }
  }

  async _updateGridPanel() {
    const gridFronts = await this.getGrids();

    if (!gridFronts.length) {
      try {
        this.store.dispatch(updateGrids([]));
        this.inspector.emit("grid-panel-updated");
        return;
      } catch (e) {
        // This call might fail if called asynchrously after the toolbox is finished
        // closing.
        return;
      }
    }

    const currentUrl = this.inspector.currentTarget.url;

    // Log how many CSS Grid elements DevTools sees.
    if (currentUrl != this.inspector.previousURL) {
      this.telemetry
        .getHistogramById(CSS_GRID_COUNT_HISTOGRAM_ID)
        .add(gridFronts.length);
      this.inspector.previousURL = currentUrl;
    }

    // Get the hostname, if there is no hostname, fall back on protocol
    // ex: `data:` uri, and `about:` pages
    const hostname =
      parseURL(currentUrl).hostname || parseURL(currentUrl).protocol;
    const customColors =
      (await asyncStorage.getItem("gridInspectorHostColors")) || {};

    const grids = [];
    for (let i = 0; i < gridFronts.length; i++) {
      const grid = gridFronts[i];
      let nodeFront = grid.containerNodeFront;

      // If the GridFront didn't yet have access to the NodeFront for its container, then
      // get it from the walker. This happens when the walker hasn't yet seen this
      // particular DOM Node in the tree yet, or when we are connected to an older server.
      if (!nodeFront) {
        try {
          nodeFront = await grid.walkerFront.getNodeFromActor(grid.actorID, [
            "containerEl",
          ]);
        } catch (e) {
          // This call might fail if called asynchrously after the toolbox is finished
          // closing.
          return;
        }
      }

      const colorForHost = customColors[hostname]
        ? customColors[hostname][i]
        : null;
      const fallbackColor = GRID_COLORS[i % GRID_COLORS.length];
      const color = this.getInitialGridColor(
        nodeFront,
        colorForHost,
        fallbackColor
      );
      const highlighted = this.highlighters.gridHighlighters.has(nodeFront);
      const disabled =
        !highlighted &&
        this.maxHighlighters > 1 &&
        this.highlighters.gridHighlighters.size === this.maxHighlighters;
      const isSubgrid = grid.isSubgrid;
      const gridData = {
        id: i,
        actorID: grid.actorID,
        color,
        disabled,
        direction: grid.direction,
        gridFragments: grid.gridFragments,
        highlighted,
        isSubgrid,
        nodeFront,
        parentNodeActorID: null,
        subgrids: [],
        writingMode: grid.writingMode,
      };

      if (
        isSubgrid &&
        (await this.inspector.currentTarget.actorHasMethod(
          "domwalker",
          "getParentGridNode"
        ))
      ) {
        let parentGridNodeFront;

        try {
          parentGridNodeFront = await nodeFront.walkerFront.getParentGridNode(
            nodeFront
          );
        } catch (e) {
          // This call might fail if called asynchrously after the toolbox is finished
          // closing.
          return;
        }

        if (!parentGridNodeFront) {
          return;
        }

        const parentIndex = grids.findIndex(
          g => g.nodeFront.actorID === parentGridNodeFront.actorID
        );
        gridData.parentNodeActorID = parentGridNodeFront.actorID;
        grids[parentIndex].subgrids.push(gridData.id);
      }

      grids.push(gridData);
    }

    // We need to make sure that nested subgrids are displayed above their parent grid
    // containers, so update the z-index of each grid before rendering them.
    for (const root of grids.filter(g => !g.parentNodeActorID)) {
      this._updateZOrder(grids, root);
    }

    this.store.dispatch(updateGrids(grids));
    this.inspector.emit("grid-panel-updated");
  }

  /**
   * Get all GridFront instances from the server(s).
   *
   *
   * @return {Array} The list of GridFronts
   */
  async getGrids() {
    let gridFronts = [];

    try {
      for (const layoutFront of this.layoutFronts) {
        gridFronts = gridFronts.concat(await layoutFront.getAllGrids());
      }
    } catch (e) {
      // This call might fail if called asynchrously after the toolbox is finished closing
    }

    return gridFronts;
  }

  /**
   * Handler for "grid-highlighter-shown" events emitted from the
   * HighlightersOverlay. Passes nodefront and event name to handleHighlighterChange.
   * Required since on and off events need the same reference object.
   *
   * @param  {NodeFront} nodeFront
   *         The NodeFront of the grid container element for which the grid
   *         highlighter is shown for.
   */
  onHighlighterShown(nodeFront) {
    this.onHighlighterChange(nodeFront, true);
  }

  /**
   * Handler for "grid-highlighter-hidden" events emitted from the
   * HighlightersOverlay. Passes nodefront and event name to handleHighlighterChange.
   * Required since on and off events need the same reference object.
   *
   * @param  {NodeFront} nodeFront
   *         The NodeFront of the grid container element for which the grid highlighter
   *         is hidden for.
   */
  onHighlighterHidden(nodeFront) {
    this.onHighlighterChange(nodeFront, false);
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
   */
  onHighlighterChange(nodeFront, highlighted) {
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
   * Handler for reflow events fired by the inspector when a node is selected. On reflows,
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
    try {
      if (!this.isPanelVisible()) {
        return;
      }

      // The list of grids currently displayed.
      const { grids } = this.store.getState();

      // The new list of grids from the server.
      const newGridFronts = await this.getGrids();

      // In some cases, the nodes for current grids may have been removed from the DOM in
      // which case we need to update.
      if (grids.length && grids.some(grid => !grid.nodeFront.actorID)) {
        await this.updateGridPanel(newGridFronts);
        return;
      }

      // Get the node front(s) from the current grid(s) so we can compare them to them to
      // the node(s) of the new grids.
      const oldNodeFronts = grids.map(grid => grid.nodeFront.actorID);
      const newNodeFronts = newGridFronts
        .filter(grid => grid.containerNode)
        .map(grid => grid.containerNodeFront.actorID);

      if (
        grids.length === newGridFronts.length &&
        oldNodeFronts.sort().join(",") == newNodeFronts.sort().join(",") &&
        !this.haveCurrentFragmentsChanged(newGridFronts)
      ) {
        // Same list of containers and the geometry of all the displayed grids remained the
        // same, we can safely abort.
        return;
      }

      // Either the list of containers or the current fragments have changed, do update.
      await this.updateGridPanel(newGridFronts);
    } catch (e) {
      this._throwUnlessDestroyed(
        e,
        "Inspector destroyed while executing onReflow callback"
      );
    }
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
    const currentUrl = this.inspector.currentTarget.url;
    // Get the hostname, if there is no hostname, fall back on protocol
    // ex: `data:` uri, and `about:` pages
    const hostname =
      parseURL(currentUrl).hostname || parseURL(currentUrl).protocol;
    const customGridColors =
      (await asyncStorage.getItem("gridInspectorHostColors")) || {};

    for (const grid of grids) {
      if (grid.nodeFront === node) {
        if (!customGridColors[hostname]) {
          customGridColors[hostname] = [];
        }
        // Update the custom color for the grid in this position.
        customGridColors[hostname][grid.id] = color;
        await asyncStorage.setItem("gridInspectorHostColors", customGridColors);

        if (!this.isPanelVisible()) {
          // This call might fail if called asynchrously after the toolbox is finished
          // closing.
          return;
        }

        // If the grid for which the color was updated currently has a highlighter, update
        // the color.
        if (this.highlighters.gridHighlighters.has(node)) {
          this.highlighters.showGridHighlighter(node);
        } else if (this.highlighters.parentGridHighlighters.has(node)) {
          this.highlighters.showParentGridHighlighter(node);
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
      this.inspector.off("reflow-in-selected-target", this.onReflow);
      return;
    }

    this.inspector.on("reflow-in-selected-target", this.onReflow);
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

  /**
   * Some grid-inspector methods are highly asynchronous and might still run
   * after the inspector was destroyed. Swallow errors if the grid inspector is
   * already destroyed, throw otherwise.
   *
   * @param {Error} error
   *        The original error object.
   * @param {String} message
   *        The message to log in case the inspector is already destroyed and
   *        the error is swallowed.
   */
  _throwUnlessDestroyed(error, message) {
    if (!this.inspector) {
      console.warn(message);
    } else {
      // If the grid inspector was not destroyed, this is an unexpected error.
      throw error;
    }
  }

  /**
   * Set z-index of each grids so that nested subgrids are always above their parent grid
   * container.
   *
   * @param {Array} grids
   *        A list of grid data.
   * @param {Object} parent
   *        A grid data of parent.
   * @param {Number} zIndex
   *        z-index for the parent.
   */
  _updateZOrder(grids, parent, zIndex = 0) {
    parent.zIndex = zIndex;

    for (const childIndex of parent.subgrids) {
      // Recurse into children grids.
      this._updateZOrder(grids, grids[childIndex], zIndex + 1);
    }
  }
}

module.exports = GridInspector;
