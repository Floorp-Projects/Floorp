/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Immutable = require("devtools/client/shared/vendor/immutable");
const Services = require("Services");

const Filters = Immutable.Record({
  error: true,
  warn: true,
  info: true,
  log: true,
  searchText: ""
});

const Prefs = Immutable.Record({
  logLimit: 1000
});

const Ui = Immutable.Record({
  configFilterBarVisible: false,
  filteredMessageVisible: false
});

module.exports.Prefs = Prefs;
module.exports.Filters = Filters;
module.exports.Ui = Ui;

const { combineReducers, createStore } = require("devtools/client/shared/vendor/redux");
const { reducers } = require("./reducers/index");

function storeFactory() {
  const initialState = {
    messages: Immutable.List(),
    prefs: new Prefs({
      logLimit: Math.max(Services.prefs.getIntPref("devtools.hud.loglimit"), 1),
    }),
    filters: new Filters({
      error: Services.prefs.getBoolPref("devtools.webconsole.filter.error"),
      warn: Services.prefs.getBoolPref("devtools.webconsole.filter.warn"),
      info: Services.prefs.getBoolPref("devtools.webconsole.filter.info"),
      log: Services.prefs.getBoolPref("devtools.webconsole.filter.log"),
      searchText: ""
    }),
    ui: new Ui({
      configFilterBarVisible: false,
      filteredMessageVisible: false
    })
  };

  return createStore(combineReducers(reducers), initialState);
}

// Provide the single store instance for app code.
module.exports.store = storeFactory();
// Provide the store factory for test code so that each test is working with
// its own instance.
module.exports.storeFactory = storeFactory;

