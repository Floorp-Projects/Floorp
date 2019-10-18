/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { services } = require("../modules/services");
const {
  FETCH_MANIFEST_FAILURE,
  FETCH_MANIFEST_START,
  FETCH_MANIFEST_SUCCESS,
  RESET_MANIFEST,
} = require("../constants");

function fetchManifest() {
  return async (dispatch, getState) => {
    dispatch({ type: FETCH_MANIFEST_START });
    const { manifest, errorMessage } = await services.fetchManifest();

    if (!errorMessage) {
      dispatch({ type: FETCH_MANIFEST_SUCCESS, manifest });
    } else {
      dispatch({ type: FETCH_MANIFEST_FAILURE, error: errorMessage });
    }
  };
}

function resetManifest() {
  return { type: RESET_MANIFEST };
}

module.exports = {
  fetchManifest,
  resetManifest,
};
