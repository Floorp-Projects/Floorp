"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getPausePoints = getPausePoints;

var _ast = require("./utils/ast");

var _types = require("@babel/types/index");

var t = _interopRequireWildcard(_types);

var _isEqual = require("devtools/client/shared/vendor/lodash").isEqual;

var _isEqual2 = _interopRequireDefault(_isEqual);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
const isForStatement = node => t.isForStatement(node) || t.isForOfStatement(node);

const isControlFlow = node => isForStatement(node) || t.isWhileStatement(node) || t.isIfStatement(node) || t.isSwitchCase(node) || t.isSwitchStatement(node) || t.isTryStatement(node) || t.isWithStatement(node);

const isAssignment = node => t.isVariableDeclarator(node) || t.isAssignmentExpression(node) || t.isAssignmentPattern(node);

const isImport = node => t.isImport(node) || t.isImportDeclaration(node);

const isReturn = node => t.isReturnStatement(node);

const isCall = node => t.isCallExpression(node) || t.isJSXElement(node);

const inStepExpression = parent => t.isArrayExpression(parent) || t.isObjectProperty(parent) || t.isCallExpression(parent) || t.isJSXElement(parent);

const inExpression = (parent, grandParent) => inStepExpression(parent) || t.isJSXAttribute(grandParent) || t.isTemplateLiteral(parent);

const isExport = node => t.isExportNamedDeclaration(node) || t.isExportDefaultDeclaration(node);

function getStartLine(node) {
  return node.loc.start.line;
}

function getPausePoints(sourceId) {
  const state = {};
  (0, _ast.traverseAst)(sourceId, {
    enter: onEnter
  }, state);
  return state;
}
/* eslint-disable complexity */


function onEnter(node, ancestors, state) {
  const parent = ancestors[ancestors.length - 1];
  const parentNode = parent && parent.node;
  const grandParent = ancestors[ancestors.length - 2];
  const startLocation = node.loc.start;

  if (isImport(node) || t.isClassDeclaration(node) || isExport(node) || t.isDebuggerStatement(node) || t.isThrowStatement(node) || t.isExpressionStatement(node) || t.isBreakStatement(node) || t.isContinueStatement(node)) {
    return addStopPoint(state, startLocation);
  }

  if (isControlFlow(node)) {
    if (isForStatement(node)) {
      addStopPoint(state, startLocation);
    } else {
      addEmptyPoint(state, startLocation);
    }

    const test = node.test || node.discriminant;

    if (test) {
      addStopPoint(state, test.loc.start);
    }

    return;
  }

  if (t.isBlockStatement(node)) {
    return addEmptyPoint(state, startLocation);
  }

  if (isReturn(node)) {
    // We do not want to pause at the return if the
    // argument is a call on the same line e.g. return foo()
    if (isCall(node.argument) && getStartLine(node) == getStartLine(node.argument)) {
      return addEmptyPoint(state, startLocation);
    }

    return addStopPoint(state, startLocation);
  }

  if (isAssignment(node)) {
    // We only want to pause at literal assignments `var a = foo()`
    const value = node.right || node.init;

    if (isCall(value) || t.isFunction(parentNode)) {
      return addEmptyPoint(state, startLocation);
    }

    return addStopPoint(state, startLocation);
  }

  if (isCall(node)) {
    let location = startLocation; // When functions are chained, we want to use the property location
    // e.g `foo().bar()`

    if (t.isMemberExpression(node.callee)) {
      location = node.callee.property.loc.start;
    } // NOTE: we do not want to land inside an expression e.g. [], {}, call


    const step = !inExpression(parent.node, grandParent && grandParent.node); // NOTE: we add a point at the beginning of the expression
    // and each of the calls because the engine does not support
    // column-based member expression calls.

    addPoint(state, startLocation, {
      break: true,
      step
    });

    if (location && !(0, _isEqual2.default)(location, startLocation)) {
      addPoint(state, location, {
        break: true,
        step
      });
    }

    return;
  }

  if (t.isClassProperty(node)) {
    return addBreakPoint(state, startLocation);
  }

  if (t.isFunction(node)) {
    const {
      line,
      column
    } = node.loc.end;
    addBreakPoint(state, startLocation);
    return addEmptyPoint(state, {
      line,
      column: column - 1
    });
  }

  if (!hasPoint(state, startLocation) && inStepExpression(parentNode)) {
    return addEmptyPoint(state, startLocation);
  }
}

function hasPoint(state, {
  line,
  column
}) {
  return state[line] && state[line][column];
}

function addPoint(state, {
  line,
  column
}, types) {
  if (!state[line]) {
    state[line] = {};
  }

  state[line][column] = types;
  return state;
}

function addStopPoint(state, location) {
  return addPoint(state, location, {
    break: true,
    step: true
  });
}

function addEmptyPoint(state, location) {
  return addPoint(state, location, {});
}

function addBreakPoint(state, location) {
  return addPoint(state, location, {
    break: true
  });
}