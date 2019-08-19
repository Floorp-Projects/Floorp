/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { UPDATE_MANIFEST } = require("../constants");

function ManifestState() {
  return {
    manifest: null,
    errorMessage: "",
  };
}

function manifestReducer(state = ManifestState(), action) {
  switch (action.type) {
    case UPDATE_MANIFEST:
      const { manifest, errorMessage } = action;
      return Object.assign({}, state, {
        manifest,
        errorMessage,
      });
    default:
      return state;
  }
}

module.exports = {
  ManifestState,
  manifestReducer,
};
