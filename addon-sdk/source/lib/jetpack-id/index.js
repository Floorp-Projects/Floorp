/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Takes parsed `package.json` manifest and returns
 * valid add-on id for it.
 */
function getID(manifest) {
  manifest = manifest || {};

  if (manifest.id) {

    if (typeof manifest.id !== "string") {
      return null;
    }

    // If manifest.id is already valid (as domain or GUID), use it
    if (isValidAOMName(manifest.id)) {
      return manifest.id;
    }
    // Otherwise, this ID is invalid so return `null`
    return null;
  }

  // If no `id` defined, turn `name` into a domain ID,
  // as we transition to `name` being an id, similar to node/npm, but
  // append a '@' to make it compatible with Firefox requirements
  if (manifest.name) {

    if (typeof manifest.name !== "string") {
      return null;
    }

    var modifiedName = "@" + manifest.name;
    return isValidAOMName(modifiedName) ? modifiedName : null;
  }

  // If no `id` or `name` property, return null as this manifest
  // is invalid
  return null;
}

module.exports = getID;

/**
 * Regex taken from XPIProvider.jsm in the Addon Manager to validate proper
 * IDs that are able to be used.
 * http://mxr.mozilla.org/mozilla-central/source/toolkit/mozapps/extensions/internal/XPIProvider.jsm#209
 */
function isValidAOMName (s) {
  return /^(\{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\}|[a-z0-9-\._]*\@[a-z0-9-\._]+)$/i.test(s || "");
}
