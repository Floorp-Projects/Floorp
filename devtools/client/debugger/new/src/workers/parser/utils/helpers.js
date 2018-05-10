"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.isFunction = isFunction;
exports.isAwaitExpression = isAwaitExpression;
exports.isYieldExpression = isYieldExpression;
exports.isObjectShorthand = isObjectShorthand;
exports.getObjectExpressionValue = getObjectExpressionValue;
exports.getVariableNames = getVariableNames;
exports.getComments = getComments;
exports.getSpecifiers = getSpecifiers;
exports.isVariable = isVariable;
exports.isComputedExpression = isComputedExpression;
exports.getMemberExpression = getMemberExpression;
exports.getVariables = getVariables;

var _types = require("@babel/types/index");

var t = _interopRequireWildcard(_types);

var _generator = require("@babel/generator/index");

var _generator2 = _interopRequireDefault(_generator);

var _flatten = require("devtools/client/shared/vendor/lodash").flatten;

var _flatten2 = _interopRequireDefault(_flatten);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function isFunction(node) {
  return t.isFunction(node) || t.isArrowFunctionExpression(node) || t.isObjectMethod(node) || t.isClassMethod(node);
}

function isAwaitExpression(path) {
  const {
    node,
    parent
  } = path;
  return t.isAwaitExpression(node) || t.isAwaitExpression(parent.init) || t.isAwaitExpression(parent);
}

function isYieldExpression(path) {
  const {
    node,
    parent
  } = path;
  return t.isYieldExpression(node) || t.isYieldExpression(parent.init) || t.isYieldExpression(parent);
}

function isObjectShorthand(parent) {
  return t.isObjectProperty(parent) && parent.key.start == parent.value.start && parent.key.loc.identifierName === parent.value.loc.identifierName;
}

function getObjectExpressionValue(node) {
  const {
    value
  } = node;

  if (t.isIdentifier(value)) {
    return value.name;
  }

  if (t.isCallExpression(value)) {
    return "";
  }

  const code = (0, _generator2.default)(value).code;
  const shouldWrap = t.isObjectExpression(value);
  return shouldWrap ? `(${code})` : code;
}

function getVariableNames(path) {
  if (t.isObjectProperty(path.node) && !isFunction(path.node.value)) {
    if (path.node.key.type === "StringLiteral") {
      return [{
        name: path.node.key.value,
        location: path.node.loc
      }];
    } else if (path.node.value.type === "Identifier") {
      return [{
        name: path.node.value.name,
        location: path.node.loc
      }];
    } else if (path.node.value.type === "AssignmentPattern") {
      return [{
        name: path.node.value.left.name,
        location: path.node.loc
      }];
    }

    return [{
      name: path.node.key.name,
      location: path.node.loc
    }];
  }

  if (!path.node.declarations) {
    return path.node.params.map(dec => ({
      name: dec.name,
      location: dec.loc
    }));
  }

  const declarations = path.node.declarations.filter(dec => dec.id.type !== "ObjectPattern").map(getVariables);
  return (0, _flatten2.default)(declarations);
}

function getComments(ast) {
  if (!ast || !ast.comments) {
    return [];
  }

  return ast.comments.map(comment => ({
    name: comment.location,
    location: comment.loc
  }));
}

function getSpecifiers(specifiers) {
  if (!specifiers) {
    return [];
  }

  return specifiers.map(specifier => specifier.local && specifier.local.name);
}

function isVariable(path) {
  const node = path.node;
  return t.isVariableDeclaration(node) || isFunction(path) && path.node.params != null && path.node.params.length || t.isObjectProperty(node) && !isFunction(path.node.value);
}

function isComputedExpression(expression) {
  return /^\[/m.test(expression);
}

function getMemberExpression(root) {
  function _getMemberExpression(node, expr) {
    if (t.isMemberExpression(node)) {
      expr = [node.property.name].concat(expr);
      return _getMemberExpression(node.object, expr);
    }

    if (t.isCallExpression(node)) {
      return [];
    }

    if (t.isThisExpression(node)) {
      return ["this"].concat(expr);
    }

    return [node.name].concat(expr);
  }

  const expr = _getMemberExpression(root, []);

  return expr.join(".");
}

function getVariables(dec) {
  if (!dec.id) {
    return [];
  }

  if (t.isArrayPattern(dec.id)) {
    if (!dec.id.elements) {
      return [];
    } // NOTE: it's possible that an element is empty
    // e.g. const [, a] = arr


    return dec.id.elements.filter(element => element).map(element => {
      return {
        name: t.isAssignmentPattern(element) ? element.left.name : element.name || element.argument.name,
        location: element.loc
      };
    });
  }

  return [{
    name: dec.id.name,
    location: dec.loc
  }];
}