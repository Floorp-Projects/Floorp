/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { autocomplete } = require("./autocomplete");
const { filters } = require("./filters");
const { messages } = require("./messages");
const { prefs } = require("./prefs");
const { ui } = require("./ui");
const { notifications } = require("./notifications");
const { history } = require("./history");

const {
  objectInspector,
} = require("devtools/client/shared/components/reps/reps.js");

exports.reducers = {
  autocomplete,
  filters,
  messages,
  prefs,
  ui,
  notifications,
  history,
  objectInspector: objectInspector.reducer.default,
};
