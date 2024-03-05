/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  MANIFEST_CATEGORIES,
  MANIFEST_ISSUE_LEVELS,
  MANIFEST_MEMBER_VALUE_TYPES,
  FETCH_MANIFEST_FAILURE,
  FETCH_MANIFEST_START,
  FETCH_MANIFEST_SUCCESS,
  RESET_MANIFEST,
} = require("resource://devtools/client/application/src/constants.js");

function _processRawManifestIcons(rawIcons) {
  // NOTE: about `rawIcons` array we are getting from platform:
  // - Icons that do not comform to the spec are filtered out
  // - We will always get a `src`
  // - We will always get `purpose` with a value (default is `["any"]`)
  // - `sizes` may be undefined
  // - `type` may be undefined
  return rawIcons.map(icon => {
    return {
      key: {
        sizes: Array.isArray(icon.sizes) ? icon.sizes.join(" ") : icon.sizes,
        contentType: icon.type,
      },
      value: {
        src: icon.src,
        purpose: icon.purpose.join(" "),
      },
      type: MANIFEST_MEMBER_VALUE_TYPES.ICON,
    };
  });
}

function _processRawManifestMembers(rawManifest) {
  function getCategoryForMember(key) {
    switch (key) {
      case "name":
      case "short_name":
        return MANIFEST_CATEGORIES.IDENTITY;
      default:
        return MANIFEST_CATEGORIES.PRESENTATION;
    }
  }

  function getValueTypeForMember(key) {
    switch (key) {
      case "start_url":
      case "scope":
        return MANIFEST_MEMBER_VALUE_TYPES.URL;
      case "theme_color":
      case "background_color":
        return MANIFEST_MEMBER_VALUE_TYPES.COLOR;
      default:
        return MANIFEST_MEMBER_VALUE_TYPES.STRING;
    }
  }

  const res = {
    [MANIFEST_CATEGORIES.IDENTITY]: [],
    [MANIFEST_CATEGORIES.PRESENTATION]: [],
  };

  // filter out extra metadata members (those with moz_ prefix) and icons
  const rawMembers = Object.entries(rawManifest).filter(
    ([key]) => !key.startsWith("moz_") && !(key === "icons")
  );

  for (const [key, value] of rawMembers) {
    const category = getCategoryForMember(key);
    const type = getValueTypeForMember(key);
    res[category].push({ key, value, type });
  }

  return res;
}

function _processRawManifestIssues(issues) {
  return issues.map(x => {
    return {
      level: x.warn
        ? MANIFEST_ISSUE_LEVELS.WARNING
        : MANIFEST_ISSUE_LEVELS.ERROR,
      message: x.warn || x.error,
      type: x.type || null,
    };
  });
}

function _processRawManifest(rawManifest) {
  const res = {
    url: rawManifest.moz_manifest_url,
  };

  // group manifest members by category
  Object.assign(res, _processRawManifestMembers(rawManifest));
  // process icons
  res.icons = _processRawManifestIcons(rawManifest.icons || []);
  // process error messages
  res.validation = _processRawManifestIssues(rawManifest.moz_validation || []);

  return res;
}

function ManifestState() {
  return {
    errorMessage: "",
    isLoading: false,
    manifest: undefined,
  };
}

function manifestReducer(state = ManifestState(), action) {
  switch (action.type) {
    case FETCH_MANIFEST_START:
      return Object.assign({}, state, {
        isLoading: true,
        mustLoadManifest: false,
      });

    case FETCH_MANIFEST_FAILURE:
      const { error } = action;
      // If we add a redux middleware to log errors, we should move the
      // console.error below there.
      console.error(error);
      return Object.assign({}, state, {
        errorMessage: error,
        isLoading: false,
        manifest: null,
      });

    case FETCH_MANIFEST_SUCCESS:
      // NOTE: we don't get an error when the page does not have a manifest,
      // but a `null` value there.
      const { manifest } = action;
      return Object.assign({}, state, {
        errorMessage: "",
        isLoading: false,
        manifest: manifest ? _processRawManifest(manifest) : null,
      });

    case RESET_MANIFEST:
      const defaultState = ManifestState();
      return defaultState;

    default:
      return state;
  }
}

module.exports = {
  ManifestState,
  manifestReducer,
};
