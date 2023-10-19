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
  addPatternIdentifiers,
  getComments,
  getCode,
  nodeLocationKey,
  getFunctionParameterNames,
} from "./utils/helpers";

import { inferClassName } from "./utils/inferClassName";
import getFunctionName from "./utils/getFunctionName";
import { getFramework } from "./frameworks";

const symbolDeclarations = new Map();

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

  if (!symbols.importsReact) {
    if (t.isImportDeclaration(path)) {
      symbols.importsReact = isReactImport(path.node);
    } else if (t.isCallExpression(path)) {
      symbols.importsReact = isReactRequire(path.node);
    }
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
      location: { start, end },
      get expression() {
        delete this.expression;
        this.expression = getSnippet(path.parentPath);
        return this.expression;
      },
    });
  }

  getIdentifierSymbols(symbols.identifiers, symbols.identifiersKeys, path);
}

function extractSymbols(sourceId) {
  const symbols = {
    functions: [],
    memberExpressions: [],
    comments: [],
    identifiers: [],
    // This holds a set of unique identifier location key (string)
    // It helps registering only the first identifier when there is duplicated ones for the same location.
    identifiersKeys: new Set(),
    classes: [],
    literals: [],
    hasJsx: false,
    hasTypes: false,
    framework: undefined,
    importsReact: false,
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

export function clearSymbols(sourceIds) {
  for (const sourceId of sourceIds) {
    symbolDeclarations.delete(sourceId);
  }
}

export function getInternalSymbols(sourceId) {
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

export function getFunctionSymbols(sourceId, maxResults) {
  const symbols = getInternalSymbols(sourceId);
  if (!symbols) {
    return [];
  }
  let { functions } = symbols;
  // Avoid transferring more symbols than necessary
  if (maxResults && functions.length > maxResults) {
    functions = functions.slice(0, maxResults);
  }
  // The Outline & the Quick open panels do not need anonymous functions
  return functions.filter(fn => fn.name !== "anonymous");
}

export function getClassSymbols(sourceId) {
  const symbols = getInternalSymbols(sourceId);
  if (!symbols) {
    return [];
  }

  return symbols.classes;
}

// This is only called from the main thread and we return a subset of attributes
export function getSymbols(sourceId) {
  const symbols = getInternalSymbols(sourceId);
  return {
    // This is used in the main thread by:
    // - Outline panel
    // - The `getFunctionSymbols` function
    // - The mapping of frame function names
    // And within the worker by `findOutOfScopeLocations`
    functions: symbols.functions,

    // The three following attributes are only used by `findBestMatchExpression` within the worker thread
    // `memberExpressions`, `literals`
    // This one is also used within the worker for framework computation
    // `identifiers`
    //
    // These three memberExpressions, literals and identifiers attributes are arrays containing objects whose attributes are:
    // * name: string
    // * location: object {start: number, end: number}
    // * expression: string
    // * computed: boolean (only for memberExpressions)
    //
    // `findBestMatchExpression` uses `location`, `computed` and `expression` (not name).
    //    `expression` isn't used from the worker thread implementation of `findBestMatchExpression`.
    //    The main thread only uses `expression` and `location`.
    // framework computation uses only:
    // * `name` for identifiers
    // * `expression` for memberExpression

    // This is used within the worker for framework computation,
    // and in the `getClassSymbols` function
    // `classes`

    // The two following are only used by the main thread for computing CodeMirror "mode"
    hasJsx: symbols.hasJsx,
    hasTypes: symbols.hasTypes,

    // This is used in the main thread only to compute the source icon
    framework: symbols.framework,

    // This is only used by `findOutOfScopeLocations`:
    // `comments`
  };
}

function getMemberExpressionSymbol(path) {
  const { start, end } = path.node.property.loc;
  return {
    location: { start, end },
    get expression() {
      delete this.expression;
      this.expression = getSnippet(path);
      return this.expression;
    },
    computed: path.node.computed,
  };
}

function isReactImport(node) {
  return (
    node.source.value == "react" &&
    node.specifiers?.some(specifier => specifier.local?.name == "React")
  );
}

function isReactRequire(node) {
  const { callee } = node;
  const name = t.isMemberExpression(callee)
    ? callee.property.name
    : callee.loc.identifierName;
  return name == "require" && node.arguments.some(arg => arg.value == "react");
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
 * @param {Array.<Object>} identifiers
 *        the current list of identifiers where to push the new identifiers
 *        related to this path.
 * @param {Set<String>} identifiersKeys
 *        List of currently registered identifier location key.
 * @param {Object} path
 */
function getIdentifierSymbols(identifiers, identifiersKeys, path) {
  if (t.isStringLiteral(path) && t.isProperty(path.parentPath)) {
    if (!identifiersKeys.has(nodeLocationKey(path.node.loc))) {
      identifiers.push({
        name: path.node.value,
        get expression() {
          delete this.expression;
          this.expression = getObjectExpressionValue(path.parent);
          return this.expression;
        },
        location: path.node.loc,
      });
    }
    return;
  }

  if (t.isIdentifier(path) && !t.isGenericTypeAnnotation(path.parent)) {
    // We want to include function params, but exclude the function name
    if (t.isClassMethod(path.parent) && !path.inList) {
      return;
    }

    if (t.isProperty(path.parentPath) && !isObjectShorthand(path.parent)) {
      if (!identifiersKeys.has(nodeLocationKey(path.node.loc))) {
        identifiers.push({
          name: path.node.name,
          get expression() {
            delete this.expression;
            this.expression = getObjectExpressionValue(path.parent);
            return this.expression;
          },
          location: path.node.loc,
        });
      }
      return;
    }

    let { start, end } = path.node.loc;
    if (path.node.typeAnnotation) {
      const { column } = path.node.typeAnnotation.loc.start;
      end = { ...end, column };
    }

    if (!identifiersKeys.has(nodeLocationKey({ start, end }))) {
      identifiers.push({
        name: path.node.name,
        expression: path.node.name,
        location: { start, end },
      });
    }
  }

  if (t.isThisExpression(path.node)) {
    if (!identifiersKeys.has(nodeLocationKey(path.node.loc))) {
      identifiers.push({
        name: "this",
        location: path.node.loc,
        expression: "this",
      });
    }
  }

  if (t.isVariableDeclarator(path)) {
    const nodeId = path.node.id;

    addPatternIdentifiers(identifiers, identifiersKeys, nodeId);
  }
}
