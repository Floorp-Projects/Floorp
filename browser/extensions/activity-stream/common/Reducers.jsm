/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {actionTypes: at} = Components.utils.import("resource://activity-stream/common/Actions.jsm", {});

const INITIAL_STATE = {
  App: {
    // Have we received real data from the app yet?
    initialized: false,
    // The locale of the browser
    locale: "",
    // Localized strings with defaults
    strings: {},
    // The version of the system-addon
    version: null
  },
  TopSites: {
    // Have we received real data from history yet?
    initialized: false,
    // The history (and possibly default) links
    rows: []
  },
  Prefs: {
    initialized: false,
    values: {}
  },
  Dialog: {
    visible: false,
    data: {}
  }
};

function App(prevState = INITIAL_STATE.App, action) {
  switch (action.type) {
    case at.INIT:
      return Object.assign({}, action.data || {}, {initialized: true});
    case at.LOCALE_UPDATED: {
      if (!action.data) {
        return prevState;
      }
      let {locale, strings} = action.data;
      return Object.assign({}, prevState, {
        locale,
        strings
      });
    }
    default:
      return prevState;
  }
}

function TopSites(prevState = INITIAL_STATE.TopSites, action) {
  let hasMatch;
  let newRows;
  switch (action.type) {
    case at.TOP_SITES_UPDATED:
      if (!action.data) {
        return prevState;
      }
      return Object.assign({}, prevState, {initialized: true, rows: action.data});
    case at.SCREENSHOT_UPDATED:
      newRows = prevState.rows.map(row => {
        if (row.url === action.data.url) {
          hasMatch = true;
          return Object.assign({}, row, {screenshot: action.data.screenshot});
        }
        return row;
      });
      return hasMatch ? Object.assign({}, prevState, {rows: newRows}) : prevState;
    case at.PLACES_BOOKMARK_ADDED:
      newRows = prevState.rows.map(site => {
        if (site.url === action.data.url) {
          const {bookmarkGuid, bookmarkTitle, lastModified} = action.data;
          return Object.assign({}, site, {bookmarkGuid, bookmarkTitle, bookmarkDateCreated: lastModified});
        }
        return site;
      });
      return Object.assign({}, prevState, {rows: newRows});
    case at.PLACES_BOOKMARK_REMOVED:
      newRows = prevState.rows.map(site => {
        if (site.url === action.data.url) {
          const newSite = Object.assign({}, site);
          delete newSite.bookmarkGuid;
          delete newSite.bookmarkTitle;
          delete newSite.bookmarkDateCreated;
          return newSite;
        }
        return site;
      });
      return Object.assign({}, prevState, {rows: newRows});
    case at.PLACES_LINK_DELETED:
    case at.PLACES_LINK_BLOCKED:
      newRows = prevState.rows.filter(val => val.url !== action.data.url);
      return Object.assign({}, prevState, {rows: newRows});
    default:
      return prevState;
  }
}

function Dialog(prevState = INITIAL_STATE.Dialog, action) {
  switch (action.type) {
    case at.DIALOG_OPEN:
      return Object.assign({}, prevState, {visible: true, data: action.data});
    case at.DIALOG_CANCEL:
      return Object.assign({}, prevState, {visible: false});
    case at.DELETE_HISTORY_URL:
      return Object.assign({}, INITIAL_STATE.Dialog);
    default:
      return prevState;
  }
}

function Prefs(prevState = INITIAL_STATE.Prefs, action) {
  let newValues;
  switch (action.type) {
    case at.PREFS_INITIAL_VALUES:
      return Object.assign({}, prevState, {initialized: true, values: action.data});
    case at.PREF_CHANGED:
      newValues = Object.assign({}, prevState.values);
      newValues[action.data.name] = action.data.value;
      return Object.assign({}, prevState, {values: newValues});
    default:
      return prevState;
  }
}

this.INITIAL_STATE = INITIAL_STATE;
this.reducers = {TopSites, App, Prefs, Dialog};

this.EXPORTED_SYMBOLS = ["reducers", "INITIAL_STATE"];
