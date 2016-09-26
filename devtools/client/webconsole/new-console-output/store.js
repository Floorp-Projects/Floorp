/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {FilterState} = require("devtools/client/webconsole/new-console-output/reducers/filters");
const {PrefState} = require("devtools/client/webconsole/new-console-output/reducers/prefs");
const { applyMiddleware, combineReducers, createStore } = require("devtools/client/shared/vendor/redux");
const { thunk } = require("devtools/client/shared/redux/middleware/thunk");
const { reducers } = require("./reducers/index");
const Services = require("Services");

function configureStore() {
  const initialState = {
    prefs: new PrefState({
      logLimit: Math.max(Services.prefs.getIntPref("devtools.hud.loglimit"), 1),
    }),
    filters: new FilterState({
      error: Services.prefs.getBoolPref("devtools.webconsole.filter.error"),
      warn: Services.prefs.getBoolPref("devtools.webconsole.filter.warn"),
      info: Services.prefs.getBoolPref("devtools.webconsole.filter.info"),
      log: Services.prefs.getBoolPref("devtools.webconsole.filter.log"),
      network: Services.prefs.getBoolPref("devtools.webconsole.filter.network"),
      netxhr: Services.prefs.getBoolPref("devtools.webconsole.filter.netxhr"),
    })
  };

  return createStore(
    combineReducers(reducers),
    initialState,
    applyMiddleware(thunk)
  );
}

// Provide the store factory for test code so that each test is working with
// its own instance.
module.exports.configureStore = configureStore;

