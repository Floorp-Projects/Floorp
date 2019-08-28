/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const actionTypes = {
  // manifest substate
  UPDATE_MANIFEST: "UPDATE_MANIFEST",
  // page substate
  UPDATE_DOMAIN: "UPDATE_DOMAIN",
  // ui substate
  UPDATE_SELECTED_PAGE: "UPDATE_SELECTED_PAGE",
  // workers substate
  UPDATE_CAN_DEBUG_WORKERS: "UPDATE_CAN_DEBUG_WORKERS",
  UPDATE_WORKERS: "UPDATE_WORKERS",
};

const PAGE_TYPES = {
  MANIFEST: "manifest",
  SERVICE_WORKERS: "service-workers",
};

const DEFAULT_PAGE = PAGE_TYPES.MANIFEST;

const MANIFEST_ISSUE_LEVELS = {
  ERROR: "error",
  WARNING: "warning",
};

// flatten constants
module.exports = Object.assign(
  {},
  {
    DEFAULT_PAGE,
    PAGE_TYPES,
    MANIFEST_ISSUE_LEVELS,
  },
  actionTypes
);
