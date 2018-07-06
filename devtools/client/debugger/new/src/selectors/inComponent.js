"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.inComponent = inComponent;

var _ = require("./index");

var _ast = require("../utils/ast");

var _ast2 = require("../reducers/ast");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function inComponent(state) {
  const selectedFrame = (0, _.getSelectedFrame)(state);

  if (!selectedFrame) {
    return;
  }

  const source = (0, _.getSource)(state, selectedFrame.location.sourceId);

  if (!source) {
    return;
  }

  const symbols = (0, _.getSymbols)(state, source);

  if (!symbols) {
    return;
  }

  const closestClass = (0, _ast.findClosestClass)(symbols, selectedFrame.location);

  if (!closestClass) {
    return null;
  }

  const sourceMetaData = (0, _ast2.getSourceMetaData)(state, source.id);

  if (!sourceMetaData || !sourceMetaData.framework) {
    return;
  }

  const inReactFile = sourceMetaData.framework == "React";
  const {
    parent
  } = closestClass;
  const isComponent = parent && parent.name.includes("Component");

  if (inReactFile && isComponent) {
    return closestClass.name;
  }
}