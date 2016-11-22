/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { Task } = require("devtools/shared/task");
const { createFactory, createElement } = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const { updateGrids } = require("./actions/grids");
const App = createFactory(require("./components/app"));
const Store = require("./store");

const { LocalizationHelper } = require("devtools/shared/l10n");
const INSPECTOR_L10N =
  new LocalizationHelper("devtools/client/locales/inspector.properties");

function LayoutView(inspector, window) {
  this.document = window.document;
  this.inspector = inspector;
  this.store = null;
  this.walker = this.inspector.walker;

  this.onGridLayoutChange = this.onGridLayoutChange.bind(this);
  this.onSidebarSelect = this.onSidebarSelect.bind(this);

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
    let provider = createElement(Provider, {
      store,
      id: "layoutview",
      title: INSPECTOR_L10N.getStr("inspector.sidebar.layoutViewTitle"),
      key: "layoutview",
    }, App());

    let defaultTab = Services.prefs.getCharPref("devtools.inspector.activeSidebar");

    this.inspector.addSidebarTab(
      "layoutview",
      INSPECTOR_L10N.getStr("inspector.sidebar.layoutViewTitle"),
      provider,
      defaultTab == "layoutview"
    );
  }),

  /**
   * Destruction function called when the inspector is destroyed. Removes event listeners
   * and cleans up references.
   */
  destroy() {
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
        nodeFront,
        gridFragments: grid.gridFragments
      });
    }

    this.store.dispatch(updateGrids(grids));
  }),

  /**
   * Handler for 'grid-layout-changed' events emitted from the LayoutActor.
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
