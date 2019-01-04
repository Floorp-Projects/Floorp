/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import * as t from "@babel/types";
import { getSymbols } from "./getSymbols";

export function getFramework(sourceId: string): ?string {
  const sourceSymbols = getSymbols(sourceId);

  if (isReactComponent(sourceSymbols)) {
    return "React";
  }
  if (isAngularComponent(sourceSymbols)) {
    return "Angular";
  }
  if (isVueComponent(sourceSymbols)) {
    return "Vue";
  }
}

// React

function isReactComponent(sourceSymbols) {
  const { imports, classes, callExpressions } = sourceSymbols;
  return (
    importsReact(imports) ||
    requiresReact(callExpressions) ||
    extendsReactComponent(classes)
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

// Angular

const isAngularComponent = sourceSymbols => {
  const { memberExpressions } = sourceSymbols;
  return memberExpressions.some(
    item =>
      item.expression == "angular.controller" ||
      item.expression == "angular.module"
  );
};

// Vue

const isVueComponent = sourceSymbols => {
  const { identifiers } = sourceSymbols;
  return identifiers.some(identifier => identifier.name == "Vue");
};
