/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import * as t from "@babel/types";

import createSimplePath from "./utils/simple-path";
import { traverseAst } from "./utils/ast";
import {
  isFunction,
  isObjectShorthand,
  isComputedExpression,
  getObjectExpressionValue,
  getPatternIdentifiers,
  getComments,
  getSpecifiers,
  getCode,
  nodeLocationKey,
  getFunctionParameterNames,
} from "./utils/helpers";

import { inferClassName } from "./utils/inferClassName";
import getFunctionName from "./utils/getFunctionName";
import { getFramework } from "./frameworks";

let symbolDeclarations = new Map();

function extractFunctionSymbol(path, state, symbols) {
  const name = getFunctionName(path.node, path.parent);

  if (!state.fnCounts[name]) {
    state.fnCounts[name] = 0;
  }
  const index = state.fnCounts[name]++;
  symbols.functions.push({
    name,
    klass: inferClassName(path),
    location: path.node.loc,
    parameterNames: getFunctionParameterNames(path),
    identifier: path.node.id,
    // indicates the occurence of the function in a file
    // e.g { name: foo, ... index: 4 } is the 4th foo function
    // in the file
    index,
  });
}

function extractSymbol(path, symbols, state) {
  if (isFunction(path)) {
    extractFunctionSymbol(path, state, symbols);
  }

  if (t.isJSXElement(path)) {
    symbols.hasJsx = true;
  }

  if (t.isGenericTypeAnnotation(path)) {
    symbols.hasTypes = true;
  }

  if (t.isClassDeclaration(path)) {
    symbols.classes.push(getClassDeclarationSymbol(path.node));
  }

  if (t.isImportDeclaration(path)) {
    symbols.imports.push(getImportDeclarationSymbol(path.node));
  }

  if (t.isObjectProperty(path)) {
    symbols.objectProperties.push(getObjectPropertySymbol(path));
  }

  if (t.isMemberExpression(path) || t.isOptionalMemberExpression(path)) {
    symbols.memberExpressions.push(getMemberExpressionSymbol(path));
  }

  if (
    (t.isStringLiteral(path) || t.isNumericLiteral(path)) &&
    t.isMemberExpression(path.parentPath)
  ) {
    // We only need literals that are part of computed memeber expressions
    const { start, end } = path.node.loc;
    symbols.literals.push({
      name: path.node.value,
      location: { start, end },
      expression: getSnippet(path.parentPath),
    });
  }

  if (t.isCallExpression(path)) {
    symbols.callExpressions.push(getCallExpressionSymbol(path.node));
  }

  symbols.identifiers.push(...getIdentifierSymbols(path));
}

function extractSymbols(sourceId) {
  const symbols = {
    functions: [],
    callExpressions: [],
    memberExpressions: [],
    objectProperties: [],
    comments: [],
    identifiers: [],
    classes: [],
    imports: [],
    literals: [],
    hasJsx: false,
    hasTypes: false,
    framework: undefined,
  };

  const state = {
    fnCounts: Object.create(null),
  };

  const ast = traverseAst(sourceId, {
    enter(node, ancestors) {
      try {
        const path = createSimplePath(ancestors);
        if (path) {
          extractSymbol(path, symbols, state);
        }
      } catch (e) {
        console.error(e);
      }
    },
  });

  // comments are extracted separately from the AST
  symbols.comments = getComments(ast);
  symbols.identifiers = getUniqueIdentifiers(symbols.identifiers);
  symbols.framework = getFramework(symbols);

  return symbols;
}

function extendSnippet(name, expression, path, prevPath) {
  const computed = path?.node.computed;
  const optional = path?.node.optional;
  const prevComputed = prevPath?.node.computed;
  const prevArray = t.isArrayExpression(prevPath);
  const array = t.isArrayExpression(path);
  const value = path?.node.property?.extra?.raw || "";

  if (expression === "") {
    if (computed) {
      return name === undefined ? `[${value}]` : `[${name}]`;
    }
    return name;
  }

  if (computed || array) {
    if (prevComputed || prevArray) {
      return `[${name}]${expression}`;
    }
    return `[${name === undefined ? value : name}].${expression}`;
  }

  if (prevComputed || prevArray) {
    return `${name}${expression}`;
  }

  if (isComputedExpression(expression) && name !== undefined) {
    return `${name}${expression}`;
  }

  if (optional) {
    return `${name}?.${expression}`;
  }

  return `${name}.${expression}`;
}

function getMemberSnippet(node, expression = "", optional = false) {
  if (t.isMemberExpression(node) || t.isOptionalMemberExpression(node)) {
    const name = t.isPrivateName(node.property)
      ? `#${node.property.id.name}`
      : node.property.name;
    const snippet = getMemberSnippet(
      node.object,
      extendSnippet(name, expression, { node }),
      node.optional
    );
    return snippet;
  }

  if (t.isCallExpression(node)) {
    return "";
  }

  if (t.isThisExpression(node)) {
    return `this.${expression}`;
  }

  if (t.isIdentifier(node)) {
    if (isComputedExpression(expression)) {
      return `${node.name}${expression}`;
    }
    if (optional) {
      return `${node.name}?.${expression}`;
    }
    return `${node.name}.${expression}`;
  }

  return expression;
}

function getObjectSnippet(path, prevPath, expression = "") {
  if (!path) {
    return expression;
  }

  const { name } = path.node.key;

  const extendedExpression = extendSnippet(name, expression, path, prevPath);

  const nextPrevPath = path;
  const nextPath = path.parentPath && path.parentPath.parentPath;

  return getSnippet(nextPath, nextPrevPath, extendedExpression);
}

function getArraySnippet(path, prevPath, expression) {
  if (!prevPath.parentPath) {
    throw new Error("Assertion failure - path should exist");
  }

  const index = `${prevPath.parentPath.containerIndex}`;
  const extendedExpression = extendSnippet(index, expression, path, prevPath);

  const nextPrevPath = path;
  const nextPath = path.parentPath && path.parentPath.parentPath;

  return getSnippet(nextPath, nextPrevPath, extendedExpression);
}

function getSnippet(path, prevPath, expression = "") {
  if (!path) {
    return expression;
  }

  if (t.isVariableDeclaration(path)) {
    const node = path.node.declarations[0];
    const { name } = node.id;
    return extendSnippet(name, expression, path, prevPath);
  }

  if (t.isVariableDeclarator(path)) {
    const node = path.node.id;
    if (t.isObjectPattern(node)) {
      return expression;
    }

    const prop = extendSnippet(node.name, expression, path, prevPath);
    return prop;
  }

  if (t.isAssignmentExpression(path)) {
    const node = path.node.left;
    const name = t.isMemberExpression(node)
      ? getMemberSnippet(node)
      : node.name;

    const prop = extendSnippet(name, expression, path, prevPath);
    return prop;
  }

  if (isFunction(path)) {
    return expression;
  }

  if (t.isIdentifier(path)) {
    return `${path.node.name}.${expression}`;
  }

  if (t.isObjectProperty(path)) {
    return getObjectSnippet(path, prevPath, expression);
  }

  if (t.isObjectExpression(path)) {
    const parentPath = prevPath?.parentPath;
    return getObjectSnippet(parentPath, prevPath, expression);
  }

  if (t.isMemberExpression(path) || t.isOptionalMemberExpression(path)) {
    return getMemberSnippet(path.node, expression);
  }

  if (t.isArrayExpression(path)) {
    if (!prevPath) {
      throw new Error("Assertion failure - path should exist");
    }

    return getArraySnippet(path, prevPath, expression);
  }

  return "";
}

export function clearSymbols() {
  symbolDeclarations = new Map();
}

export function getSymbols(sourceId) {
  if (symbolDeclarations.has(sourceId)) {
    const symbols = symbolDeclarations.get(sourceId);
    if (symbols) {
      return symbols;
    }
  }

  const symbols = extractSymbols(sourceId);

  symbolDeclarations.set(sourceId, symbols);
  return symbols;
}

function getUniqueIdentifiers(identifiers) {
  const newIdentifiers = [];
  const locationKeys = new Set();
  for (const newId of identifiers) {
    const key = nodeLocationKey(newId);
    if (!locationKeys.has(key)) {
      locationKeys.add(key);
      newIdentifiers.push(newId);
    }
  }

  return newIdentifiers;
}

function getMemberExpressionSymbol(path) {
  const { start, end } = path.node.property.loc;
  return {
    name: t.isPrivateName(path.node.property)
      ? `#${path.node.property.id.name}`
      : path.node.property.name,
    location: { start, end },
    expression: getSnippet(path),
    computed: path.node.computed,
  };
}

function getImportDeclarationSymbol(node) {
  return {
    source: node.source.value,
    location: node.loc,
    specifiers: getSpecifiers(node.specifiers),
  };
}

function getObjectPropertySymbol(path) {
  const { start, end, identifierName } = path.node.key.loc;
  return {
    name: identifierName,
    location: { start, end },
    expression: getSnippet(path),
  };
}

function getCallExpressionSymbol(node) {
  const { callee, arguments: args } = node;
  const values = args.filter(arg => arg.value).map(arg => arg.value);
  if (t.isMemberExpression(callee)) {
    const {
      property: { name, loc },
    } = callee;
    return {
      name,
      values,
      location: loc,
    };
  }
  const { start, end, identifierName } = callee.loc;
  return {
    name: identifierName,
    values,
    location: { start, end },
  };
}

function getClassParentName(superClass) {
  return t.isMemberExpression(superClass)
    ? getCode(superClass)
    : superClass.name;
}

function getClassParentSymbol(superClass) {
  if (!superClass) {
    return null;
  }
  return {
    name: getClassParentName(superClass),
    location: superClass.loc,
  };
}

function getClassDeclarationSymbol(node) {
  const { loc, superClass } = node;
  return {
    name: node.id.name,
    parent: getClassParentSymbol(superClass),
    location: loc,
  };
}

/**
 * Get a list of identifiers that are part of the given path.
 *
 * @param {Object} path
 * @returns {Array.<Object>} a list of identifiers
 */
function getIdentifierSymbols(path) {
  if (t.isStringLiteral(path) && t.isProperty(path.parentPath)) {
    const { start, end } = path.node.loc;
    return [
      {
        name: path.node.value,
        expression: getObjectExpressionValue(path.parent),
        location: { start, end },
      },
    ];
  }

  const identifiers = [];
  if (t.isIdentifier(path) && !t.isGenericTypeAnnotation(path.parent)) {
    // We want to include function params, but exclude the function name
    if (t.isClassMethod(path.parent) && !path.inList) {
      return [];
    }

    if (t.isProperty(path.parentPath) && !isObjectShorthand(path.parent)) {
      const { start, end } = path.node.loc;
      return [
        {
          name: path.node.name,
          expression: getObjectExpressionValue(path.parent),
          location: { start, end },
        },
      ];
    }

    let { start, end } = path.node.loc;
    if (path.node.typeAnnotation) {
      const { column } = path.node.typeAnnotation.loc.start;
      end = { ...end, column };
    }

    identifiers.push({
      name: path.node.name,
      expression: path.node.name,
      location: { start, end },
    });
  }

  if (t.isThisExpression(path.node)) {
    const { start, end } = path.node.loc;
    identifiers.push({
      name: "this",
      location: { start, end },
      expression: "this",
    });
  }

  if (t.isVariableDeclarator(path)) {
    const nodeId = path.node.id;

    identifiers.push(...getPatternIdentifiers(nodeId));
  }

  return identifiers;
}
