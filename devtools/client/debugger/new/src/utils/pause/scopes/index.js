"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getScopes = getScopes;

var _getScope = require("./getScope");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getScopes(why, selectedFrame, frameScopes) {
  if (!why || !selectedFrame) {
    return null;
  }

  if (!frameScopes) {
    return null;
  }

  const scopes = [];
  let scope = frameScopes;
  let scopeIndex = 1;

  while (scope) {
    const scopeItem = (0, _getScope.getScope)(scope, selectedFrame, frameScopes, why, scopeIndex);

    if (scopeItem) {
      scopes.push(scopeItem);
    }

    scopeIndex++;
    scope = scope.parent;
  }

  return scopes;
}