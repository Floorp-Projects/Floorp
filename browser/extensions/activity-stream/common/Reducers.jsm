/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {actionTypes: at} = Components.utils.import("resource://activity-stream/common/Actions.jsm", {});

const INITIAL_STATE = {
  TopSites: {
    init: false,
    rows: []
  },
  Search: {
    currentEngine: {
      name: "",
      icon: ""
    },
    engines: []
  }
};

// TODO: Handle some real actions here, once we have a TopSites feed working
function TopSites(prevState = INITIAL_STATE.TopSites, action) {
  let hasMatch;
  let newRows;
  switch (action.type) {
    case at.TOP_SITES_UPDATED:
      if (!action.data) {
        return prevState;
      }
      return Object.assign({}, prevState, {init: true, rows: action.data});
    case at.SCREENSHOT_UPDATED:
      newRows = prevState.rows.map(row => {
        if (row.url === action.data.url) {
          hasMatch = true;
          return Object.assign({}, row, {screenshot: action.data.screenshot});
        }
        return row;
      });
      return hasMatch ? Object.assign({}, prevState, {rows: newRows}) : prevState;
    default:
      return prevState;
  }
}

function Search(prevState = INITIAL_STATE.Search, action) {
  switch (action.type) {
    case at.SEARCH_STATE_UPDATED: {
      if (!action.data) {
        return prevState;
      }
      let {currentEngine, engines} = action.data;
      return Object.assign({}, prevState, {
        currentEngine,
        engines
      });
    }
    default:
      return prevState;
  }
}
this.INITIAL_STATE = INITIAL_STATE;
this.reducers = {TopSites, Search};

this.EXPORTED_SYMBOLS = ["reducers", "INITIAL_STATE"];
