"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getASTLocation = getASTLocation;
exports.findScopeByName = findScopeByName;

var _parser = require("../../workers/parser/index");

var _ast = require("../ast");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getASTLocation(source, symbols, location) {
  if (source.isWasm || !symbols || symbols.loading) {
    return {
      name: undefined,
      offset: location
    };
  }

  const scope = (0, _ast.findClosestFunction)(symbols, location);

  if (scope) {
    // we only record the line, but at some point we may
    // also do column offsets
    const line = location.line - scope.location.start.line;
    return {
      name: scope.name,
      offset: {
        line,
        column: undefined
      }
    };
  }

  return {
    name: undefined,
    offset: location
  };
}

async function findScopeByName(source, name) {
  const symbols = await (0, _parser.getSymbols)(source.id);
  const functions = symbols.functions;
  return functions.find(node => node.name === name);
}