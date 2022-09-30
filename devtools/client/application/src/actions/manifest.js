/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  l10n,
} = require("resource://devtools/client/application/src/modules/l10n.js");

const {
  services,
  ManifestDevToolsError,
} = require("resource://devtools/client/application/src/modules/application-services.js");
const {
  FETCH_MANIFEST_FAILURE,
  FETCH_MANIFEST_START,
  FETCH_MANIFEST_SUCCESS,
  RESET_MANIFEST,
} = require("resource://devtools/client/application/src/constants.js");

function fetchManifest() {
  return async ({ dispatch, getState }) => {
    dispatch({ type: FETCH_MANIFEST_START });
    try {
      const manifest = await services.fetchManifest();
      dispatch({ type: FETCH_MANIFEST_SUCCESS, manifest });
    } catch (error) {
      let errorMessage = error.message;

      // since Firefox DevTools errors may not make sense for the user, swap
      // their message for a generic one.
      if (error instanceof ManifestDevToolsError) {
        console.error(error);
        errorMessage = l10n.getString("manifest-loaded-devtools-error");
      }

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
