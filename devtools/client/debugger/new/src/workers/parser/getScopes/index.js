"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.buildScopeList = undefined;
exports.default = getScopes;
exports.clearScopes = clearScopes;

var _visitor = require("./visitor");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
let parsedScopesCache = new Map();

function getScopes(location) {
  const {
    sourceId
  } = location;
  let parsedScopes = parsedScopesCache.get(sourceId);

  if (!parsedScopes) {
    parsedScopes = (0, _visitor.parseSourceScopes)(sourceId);
    parsedScopesCache.set(sourceId, parsedScopes);
  }

  return parsedScopes ? findScopes(parsedScopes, location) : [];
}

function clearScopes() {
  parsedScopesCache = new Map();
}

exports.buildScopeList = _visitor.buildScopeList;
/**
 * Searches all scopes and their bindings at the specific location.
 */

function findScopes(scopes, location) {
  // Find inner most in the tree structure.
  let searchInScopes = scopes;
  const found = [];

  while (searchInScopes) {
    const foundOne = searchInScopes.some(s => {
      if (compareLocations(s.start, location) <= 0 && compareLocations(location, s.end) < 0) {
        // Found the next scope, trying to search recusevly in its children.
        found.unshift(s);
        searchInScopes = s.children;
        return true;
      }

      return false;
    });

    if (!foundOne) {
      break;
    }
  }

  return found.map(i => {
    return {
      type: i.type,
      displayName: i.displayName,
      start: i.start,
      end: i.end,
      bindings: i.bindings
    };
  });
}

function compareLocations(a, b) {
  // According to type of Location.column can be undefined, if will not be the
  // case here, ignoring flow error.
  // $FlowIgnore
  return a.line == b.line ? a.column - b.column : a.line - b.line;
}