/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import * as t from "@babel/types";

export function getFramework(symbols) {
  if (isReactComponent(symbols)) {
    return "React";
  }
  if (isAngularComponent(symbols)) {
    return "Angular";
  }
  if (isVueComponent(symbols)) {
    return "Vue";
  }

  return null;
}

function isReactComponent({ imports, classes, callExpressions, identifiers }) {
  return (
    importsReact(imports) ||
    requiresReact(callExpressions) ||
    extendsReactComponent(classes) ||
    isReact(identifiers) ||
    isRedux(identifiers)
  );
}

function importsReact(imports) {
  return imports.some(
    importObj =>
      importObj.source === "react" &&
      importObj.specifiers.some(specifier => specifier === "React")
  );
}

function requiresReact(callExpressions) {
  return callExpressions.some(
    callExpression =>
      callExpression.name === "require" &&
      callExpression.values.some(value => value === "react")
  );
}

function extendsReactComponent(classes) {
  return classes.some(
    classObj =>
      t.isIdentifier(classObj.parent, { name: "Component" }) ||
      t.isIdentifier(classObj.parent, { name: "PureComponent" }) ||
      (t.isMemberExpression(classObj.parent, { computed: false }) &&
        t.isIdentifier(classObj.parent, { name: "Component" }))
  );
}

function isAngularComponent({ memberExpressions }) {
  return memberExpressions.some(
    item =>
      item.expression == "angular.controller" ||
      item.expression == "angular.module"
  );
}

function isVueComponent({ identifiers }) {
  return identifiers.some(identifier => identifier.name == "Vue");
}

/* This identifies the react lib file */
function isReact(identifiers) {
  return identifiers.some(identifier => identifier.name == "isReactComponent");
}

/* This identifies the redux lib file */
function isRedux(identifiers) {
  return identifiers.some(identifier => identifier.name == "Redux");
}
