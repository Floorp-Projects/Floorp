/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Find Placeholders in the template and save them along with their
 * paths.
 */
function findPlaceholders(template, constructor, path = [], placeholders = []) {
  if (!template || typeof template != "object") {
    return placeholders;
  }

  if (template instanceof constructor) {
    placeholders.push({ placeholder: template, path: [...path] });
    return placeholders;
  }

  for (const name in template) {
    path.push(name);
    findPlaceholders(template[name], constructor, path, placeholders);
    path.pop();
  }

  return placeholders;
}

exports.findPlaceholders = findPlaceholders;

/**
 * Get the value at a given path, or undefined if not found.
 */
function getPath(obj, path) {
  for (const name of path) {
    if (!(name in obj)) {
      return undefined;
    }
    obj = obj[name];
  }
  return obj;
}
exports.getPath = getPath;
