"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getFramework = getFramework;

var _types = require("@babel/types/index");

var t = _interopRequireWildcard(_types);

var _getSymbols = require("./getSymbols");

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function getFramework(sourceId) {
  const sourceSymbols = (0, _getSymbols.getSymbols)(sourceId);

  if (isReactComponent(sourceSymbols)) {
    return "React";
  }

  if (isAngularComponent(sourceSymbols)) {
    return "Angular";
  }

  if (isVueComponent(sourceSymbols)) {
    return "Vue";
  }
} // React


function isReactComponent(sourceSymbols) {
  const {
    imports,
    classes,
    callExpressions
  } = sourceSymbols;
  return importsReact(imports) || requiresReact(callExpressions) || extendsReactComponent(classes);
}

function importsReact(imports) {
  return imports.some(importObj => importObj.source === "react" && importObj.specifiers.some(specifier => specifier === "React"));
}

function requiresReact(callExpressions) {
  return callExpressions.some(callExpression => callExpression.name === "require" && callExpression.values.some(value => value === "react"));
}

function extendsReactComponent(classes) {
  return classes.some(classObj => t.isIdentifier(classObj.parent, {
    name: "Component"
  }) || t.isIdentifier(classObj.parent, {
    name: "PureComponent"
  }) || t.isMemberExpression(classObj.parent, {
    computed: false
  }) && t.isIdentifier(classObj.parent, {
    name: "Component"
  }));
} // Angular


const isAngularComponent = sourceSymbols => {
  const {
    memberExpressions,
    identifiers
  } = sourceSymbols;
  return identifiesAngular(identifiers) && hasAngularExpressions(memberExpressions);
};

const identifiesAngular = identifiers => {
  return identifiers.some(item => item.name == "angular");
};

const hasAngularExpressions = memberExpressions => {
  return memberExpressions.some(item => item.name == "controller" || item.name == "module");
}; // Vue


const isVueComponent = sourceSymbols => {
  const {
    identifiers
  } = sourceSymbols;
  return identifiers.some(identifier => identifier.name == "Vue");
};