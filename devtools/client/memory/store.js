/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const createStore = require("devtools/client/shared/redux/create-store");
const reducers = require("devtools/client/memory/reducers");

module.exports = () =>
  createStore(reducers, {
    enableTaskMiddleware: true,
    // Uncomment this for logging in tests.
    // shouldLog: true,
  });
