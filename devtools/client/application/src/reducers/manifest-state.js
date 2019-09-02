/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  MANIFEST_CATEGORIES,
  MANIFEST_ISSUE_LEVELS,
  UPDATE_MANIFEST,
} = require("../constants");

function _processRawManifestIcons(rawIcons) {
  // TODO: we are currently ignoring the following fields present in the data
  // from the canonical manifest:
  // - purpose
  // - type
  // We should include this info so we can pass it as props to the relevant
  // component
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1577446
  return rawIcons.map(icon => {
    return {
      key: icon.sizes.join(" "),
      value: icon.src,
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

  const res = {
    [MANIFEST_CATEGORIES.IDENTITY]: [],
    [MANIFEST_CATEGORIES.PRESENTATION]: [],
  };

  // filter out extra metadata members (those with moz_ prefix) and icons
  const rawMembers = Object.entries(rawManifest).filter(
    ([key, value]) => !key.startsWith("moz_") && !(key === "icons")
  );

  for (const [key, value] of rawMembers) {
    const category = getCategoryForMember(key);
    res[category].push({ key, value });
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
    manifest: null,
    errorMessage: "",
  };
}

function manifestReducer(state = ManifestState(), action) {
  switch (action.type) {
    case UPDATE_MANIFEST:
      const { manifest, errorMessage } = action;
      return Object.assign({}, state, {
        manifest: manifest ? _processRawManifest(manifest) : null,
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
