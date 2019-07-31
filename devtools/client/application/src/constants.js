/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const actionTypes = {
  // page substate
  UPDATE_DOMAIN: "UPDATE_DOMAIN",
  // ui substate
  UPDATE_SELECTED_PAGE: "UPDATE_SELECTED_PAGE",
  // workers substate
  UPDATE_CAN_DEBUG_WORKERS: "UPDATE_CAN_DEBUG_WORKERS",
  UPDATE_WORKERS: "UPDATE_WORKERS",
};

const PAGE_TYPES = {
  SERVICE_WORKERS: "service-workers",
};

const DEFAULT_PAGE = PAGE_TYPES.SERVICE_WORKERS;

// flatten constants
module.exports = Object.assign({}, { DEFAULT_PAGE, PAGE_TYPES }, actionTypes);
