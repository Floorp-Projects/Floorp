"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.parseSourceScopes = parseSourceScopes;
exports.buildScopeList = buildScopeList;

var _isEmpty = require("devtools/client/shared/vendor/lodash").isEmpty;

var _isEmpty2 = _interopRequireDefault(_isEmpty);

var _types = require("@babel/types/index");

var t = _interopRequireWildcard(_types);

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

var _getFunctionName = require("../utils/getFunctionName");

var _getFunctionName2 = _interopRequireDefault(_getFunctionName);

var _ast = require("../utils/ast");

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function parseSourceScopes(sourceId) {
  const ast = (0, _ast.getAst)(sourceId);

  if ((0, _isEmpty2.default)(ast)) {
    return null;
  }

  return buildScopeList(ast, sourceId);
}

function buildScopeList(ast, sourceId) {
  const {
    global,
    lexical
  } = createGlobalScope(ast, sourceId);
  const state = {
    sourceId,
    freeVariables: new Map(),
    freeVariableStack: [],
    scope: lexical,
    scopeStack: [],
    declarationBindingIds: new Set()
  };
  t.traverse(ast, scopeCollectionVisitor, state);

  for (const [key, freeVariables] of state.freeVariables) {
    let binding = global.bindings[key];

    if (!binding) {
      binding = {
        type: "global",
        refs: []
      };
      global.bindings[key] = binding;
    }

    binding.refs = freeVariables.concat(binding.refs);
  } // TODO: This should probably check for ".mjs" extension on the
  // original file, and should also be skipped if the the generated
  // code is an ES6 module rather than a script.


  if ((0, _devtoolsSourceMap.isGeneratedId)(sourceId) || ast.program.sourceType === "script" && !looksLikeCommonJS(global)) {
    stripModuleScope(global);
  }

  return toParsedScopes([global], sourceId) || [];
}

function toParsedScopes(children, sourceId) {
  if (!children || children.length === 0) {
    return undefined;
  }

  return children.map(scope => {
    // Removing unneed information from TempScope such as parent reference.
    // We also need to convert BabelLocation to the Location type.
    return {
      start: scope.loc.start,
      end: scope.loc.end,
      type: scope.type === "module" ? "block" : scope.type,
      displayName: scope.displayName,
      bindings: scope.bindings,
      children: toParsedScopes(scope.children, sourceId)
    };
  });
}

function createTempScope(type, displayName, parent, loc) {
  const result = {
    type,
    displayName,
    parent,
    children: [],
    loc,
    bindings: Object.create(null)
  };

  if (parent) {
    parent.children.push(result);
  }

  return result;
}

function pushTempScope(state, type, displayName, loc) {
  const scope = createTempScope(type, displayName, state.scope, loc);
  state.scope = scope;
  state.freeVariableStack.push(state.freeVariables);
  state.freeVariables = new Map();
  return scope;
}

function isNode(node, type) {
  return node ? node.type === type : false;
}

function getVarScope(scope) {
  let s = scope;

  while (s.type !== "function" && s.type !== "module") {
    if (!s.parent) {
      return s;
    }

    s = s.parent;
  }

  return s;
}

function fromBabelLocation(location, sourceId) {
  return {
    sourceId,
    line: location.line,
    column: location.column
  };
}

function parseDeclarator(declaratorId, targetScope, type, locationType, declaration, state) {
  if (isNode(declaratorId, "Identifier")) {
    let existing = targetScope.bindings[declaratorId.name];

    if (!existing) {
      existing = {
        type,
        refs: []
      };
      targetScope.bindings[declaratorId.name] = existing;
    }

    state.declarationBindingIds.add(declaratorId);
    existing.refs.push({
      type: locationType,
      start: fromBabelLocation(declaratorId.loc.start, state.sourceId),
      end: fromBabelLocation(declaratorId.loc.end, state.sourceId),
      declaration: {
        start: fromBabelLocation(declaration.loc.start, state.sourceId),
        end: fromBabelLocation(declaration.loc.end, state.sourceId)
      }
    });
  } else if (isNode(declaratorId, "ObjectPattern")) {
    declaratorId.properties.forEach(prop => {
      parseDeclarator(prop.value, targetScope, type, locationType, declaration, state);
    });
  } else if (isNode(declaratorId, "ArrayPattern")) {
    declaratorId.elements.forEach(item => {
      parseDeclarator(item, targetScope, type, locationType, declaration, state);
    });
  } else if (isNode(declaratorId, "AssignmentPattern")) {
    parseDeclarator(declaratorId.left, targetScope, type, locationType, declaration, state);
  } else if (isNode(declaratorId, "RestElement")) {
    parseDeclarator(declaratorId.argument, targetScope, type, locationType, declaration, state);
  }
}

function isLetOrConst(node) {
  return node.kind === "let" || node.kind === "const";
}

function hasLexicalDeclaration(node, parent) {
  const isFunctionBody = t.isFunction(parent, {
    body: node
  });
  return node.body.some(child => isLexicalVariable(child) || !isFunctionBody && child.type === "FunctionDeclaration" || child.type === "ClassDeclaration");
}

function isLexicalVariable(node) {
  return isNode(node, "VariableDeclaration") && isLetOrConst(node);
}

function createGlobalScope(ast, sourceId) {
  const global = createTempScope("object", "Global", null, {
    start: fromBabelLocation(ast.loc.start, sourceId),
    end: fromBabelLocation(ast.loc.end, sourceId)
  });
  const lexical = createTempScope("block", "Lexical Global", global, {
    start: fromBabelLocation(ast.loc.start, sourceId),
    end: fromBabelLocation(ast.loc.end, sourceId)
  });
  return {
    global,
    lexical
  };
}

const scopeCollectionVisitor = {
  // eslint-disable-next-line complexity
  enter(node, ancestors, state) {
    state.scopeStack.push(state.scope);
    const parentNode = ancestors.length === 0 ? null : ancestors[ancestors.length - 1].node;

    if (t.isProgram(node)) {
      const scope = pushTempScope(state, "module", "Module", {
        start: fromBabelLocation(node.loc.start, state.sourceId),
        end: fromBabelLocation(node.loc.end, state.sourceId)
      });
      scope.bindings.this = {
        type: "implicit",
        refs: []
      };
    } else if (t.isFunction(node)) {
      let scope = state.scope;

      if (t.isFunctionExpression(node) && isNode(node.id, "Identifier")) {
        scope = pushTempScope(state, "block", "Function Expression", {
          start: fromBabelLocation(node.loc.start, state.sourceId),
          end: fromBabelLocation(node.loc.end, state.sourceId)
        });
        state.declarationBindingIds.add(node.id);
        scope.bindings[node.id.name] = {
          type: "const",
          refs: [{
            type: "fn-expr",
            start: fromBabelLocation(node.id.loc.start, state.sourceId),
            end: fromBabelLocation(node.id.loc.end, state.sourceId),
            declaration: {
              start: fromBabelLocation(node.loc.start, state.sourceId),
              end: fromBabelLocation(node.loc.end, state.sourceId)
            }
          }]
        };
      }

      if (t.isFunctionDeclaration(node) && isNode(node.id, "Identifier")) {
        // This ignores Annex B function declaration hoisting, which
        // is probably a fine assumption.
        state.declarationBindingIds.add(node.id);
        const fnScope = getVarScope(scope);
        scope.bindings[node.id.name] = {
          type: fnScope === scope ? "var" : "let",
          refs: [{
            type: "fn-decl",
            start: fromBabelLocation(node.id.loc.start, state.sourceId),
            end: fromBabelLocation(node.id.loc.end, state.sourceId),
            declaration: {
              start: fromBabelLocation(node.loc.start, state.sourceId),
              end: fromBabelLocation(node.loc.end, state.sourceId)
            }
          }]
        };
      }

      scope = pushTempScope(state, "function", (0, _getFunctionName2.default)(node, parentNode), {
        // Being at the start of a function doesn't count as
        // being inside of it.
        start: fromBabelLocation(node.params[0] ? node.params[0].loc.start : node.loc.start, state.sourceId),
        end: fromBabelLocation(node.loc.end, state.sourceId)
      });
      node.params.forEach(param => parseDeclarator(param, scope, "var", "fn-param", node, state));

      if (!t.isArrowFunctionExpression(node)) {
        scope.bindings.this = {
          type: "implicit",
          refs: []
        };
        scope.bindings.arguments = {
          type: "implicit",
          refs: []
        };
      }
    } else if (t.isClass(node)) {
      if (t.isIdentifier(node.id)) {
        // For decorated classes, the AST considers the first the decorator
        // to be the start of the class. For the purposes of mapping class
        // declarations however, we really want to look for the "class Foo"
        // piece. To achieve that, we estimate the location of the declaration
        // instead.
        let declStart = node.loc.start;

        if (node.decorators && node.decorators.length > 0) {
          // Estimate the location of the "class" keyword since it
          // is unlikely to be a different line than the class name.
          declStart = {
            line: node.id.loc.start.line,
            column: node.id.loc.start.column - "class ".length
          };
        }

        const declaration = {
          start: fromBabelLocation(declStart, state.sourceId),
          end: fromBabelLocation(node.loc.end, state.sourceId)
        };

        if (t.isClassDeclaration(node)) {
          state.declarationBindingIds.add(node.id);
          state.scope.bindings[node.id.name] = {
            type: "let",
            refs: [{
              type: "class-decl",
              start: fromBabelLocation(node.id.loc.start, state.sourceId),
              end: fromBabelLocation(node.id.loc.end, state.sourceId),
              declaration
            }]
          };
        }

        const scope = pushTempScope(state, "block", "Class", {
          start: fromBabelLocation(node.loc.start, state.sourceId),
          end: fromBabelLocation(node.loc.end, state.sourceId)
        });
        state.declarationBindingIds.add(node.id);
        scope.bindings[node.id.name] = {
          type: "const",
          refs: [{
            type: "class-inner",
            start: fromBabelLocation(node.id.loc.start, state.sourceId),
            end: fromBabelLocation(node.id.loc.end, state.sourceId),
            declaration
          }]
        };
      }
    } else if (t.isForXStatement(node) || t.isForStatement(node)) {
      const init = node.init || node.left;

      if (isNode(init, "VariableDeclaration") && isLetOrConst(init)) {
        // Debugger will create new lexical environment for the for.
        pushTempScope(state, "block", "For", {
          // Being at the start of a for loop doesn't count as
          // being inside it.
          start: fromBabelLocation(init.loc.start, state.sourceId),
          end: fromBabelLocation(node.loc.end, state.sourceId)
        });
      }
    } else if (t.isCatchClause(node)) {
      const scope = pushTempScope(state, "block", "Catch", {
        start: fromBabelLocation(node.loc.start, state.sourceId),
        end: fromBabelLocation(node.loc.end, state.sourceId)
      });
      parseDeclarator(node.param, scope, "var", "catch", node, state);
    } else if (t.isBlockStatement(node) && hasLexicalDeclaration(node, parentNode)) {
      // Debugger will create new lexical environment for the block.
      pushTempScope(state, "block", "Block", {
        start: fromBabelLocation(node.loc.start, state.sourceId),
        end: fromBabelLocation(node.loc.end, state.sourceId)
      });
    } else if (t.isVariableDeclaration(node) && (node.kind === "var" || // Lexical declarations in for statements are handled above.
    !t.isForStatement(parentNode, {
      init: node
    }) || !t.isForXStatement(parentNode, {
      left: node
    }))) {
      // Finds right lexical environment
      const hoistAt = !isLetOrConst(node) ? getVarScope(state.scope) : state.scope;
      node.declarations.forEach(declarator => {
        parseDeclarator(declarator.id, hoistAt, node.kind, node.kind, node, state);
      });
    } else if (t.isImportDeclaration(node) && (!node.importKind || node.importKind === "value")) {
      node.specifiers.forEach(spec => {
        if (t.isImportNamespaceSpecifier(spec)) {
          state.declarationBindingIds.add(spec.local);
          state.scope.bindings[spec.local.name] = {
            // Imported namespaces aren't live import bindings, they are
            // just normal const bindings.
            type: "const",
            refs: [{
              type: "import-ns-decl",
              start: fromBabelLocation(spec.local.loc.start, state.sourceId),
              end: fromBabelLocation(spec.local.loc.end, state.sourceId),
              declaration: {
                start: fromBabelLocation(node.loc.start, state.sourceId),
                end: fromBabelLocation(node.loc.end, state.sourceId)
              }
            }]
          };
        } else {
          state.declarationBindingIds.add(spec.local);
          state.scope.bindings[spec.local.name] = {
            type: "import",
            refs: [{
              type: "import-decl",
              start: fromBabelLocation(spec.local.loc.start, state.sourceId),
              end: fromBabelLocation(spec.local.loc.end, state.sourceId),
              importName: t.isImportDefaultSpecifier(spec) ? "default" : spec.imported.name,
              declaration: {
                start: fromBabelLocation(node.loc.start, state.sourceId),
                end: fromBabelLocation(node.loc.end, state.sourceId)
              }
            }]
          };
        }
      });
    } else if (t.isTSEnumDeclaration(node)) {
      state.declarationBindingIds.add(node.id);
      state.scope.bindings[node.id.name] = {
        type: "const",
        refs: [{
          type: "ts-enum-decl",
          start: fromBabelLocation(node.id.loc.start, state.sourceId),
          end: fromBabelLocation(node.id.loc.end, state.sourceId),
          declaration: {
            start: fromBabelLocation(node.loc.start, state.sourceId),
            end: fromBabelLocation(node.loc.end, state.sourceId)
          }
        }]
      };
    } else if (t.isIdentifier(node) && t.isReferenced(node, parentNode) && // Babel doesn't cover this in 'isReferenced' yet, but it should
    // eventually.
    !t.isTSEnumMember(parentNode, {
      id: node
    }) && // isReferenced above fails to see `var { foo } = ...` as a non-reference
    // because the direct parent is not enough to know that the pattern is
    // used within a variable declaration.
    !state.declarationBindingIds.has(node)) {
      let freeVariables = state.freeVariables.get(node.name);

      if (!freeVariables) {
        freeVariables = [];
        state.freeVariables.set(node.name, freeVariables);
      }

      freeVariables.push({
        type: "ref",
        start: fromBabelLocation(node.loc.start, state.sourceId),
        end: fromBabelLocation(node.loc.end, state.sourceId),
        meta: buildMetaBindings(state.sourceId, node, ancestors)
      });
    } else if (isOpeningJSXIdentifier(node, ancestors)) {
      let freeVariables = state.freeVariables.get(node.name);

      if (!freeVariables) {
        freeVariables = [];
        state.freeVariables.set(node.name, freeVariables);
      }

      freeVariables.push({
        type: "ref",
        start: fromBabelLocation(node.loc.start, state.sourceId),
        end: fromBabelLocation(node.loc.end, state.sourceId),
        meta: buildMetaBindings(state.sourceId, node, ancestors)
      });
    } else if (t.isThisExpression(node)) {
      let freeVariables = state.freeVariables.get("this");

      if (!freeVariables) {
        freeVariables = [];
        state.freeVariables.set("this", freeVariables);
      }

      freeVariables.push({
        type: "ref",
        start: fromBabelLocation(node.loc.start, state.sourceId),
        end: fromBabelLocation(node.loc.end, state.sourceId),
        meta: buildMetaBindings(state.sourceId, node, ancestors)
      });
    } else if (t.isClassProperty(parentNode, {
      value: node
    })) {
      const scope = pushTempScope(state, "function", "Class Field", {
        start: fromBabelLocation(node.loc.start, state.sourceId),
        end: fromBabelLocation(node.loc.end, state.sourceId)
      });
      scope.bindings.this = {
        type: "implicit",
        refs: []
      };
      scope.bindings.arguments = {
        type: "implicit",
        refs: []
      };
    } else if (t.isSwitchStatement(node) && node.cases.some(caseNode => caseNode.consequent.some(child => isLexicalVariable(child)))) {
      pushTempScope(state, "block", "Switch", {
        start: fromBabelLocation(node.loc.start, state.sourceId),
        end: fromBabelLocation(node.loc.end, state.sourceId)
      });
    }
  },

  exit(node, ancestors, state) {
    const currentScope = state.scope;
    const parentScope = state.scopeStack.pop();

    if (!parentScope) {
      throw new Error("Assertion failure - unsynchronized pop");
    }

    state.scope = parentScope; // It is possible, as in the case of function expressions, that a single
    // node has added multiple scopes, so we need to traverse upward here
    // rather than jumping stright to 'parentScope'.

    for (let scope = currentScope; scope && scope !== parentScope; scope = scope.parent) {
      const freeVariables = state.freeVariables;
      state.freeVariables = state.freeVariableStack.pop();
      const parentFreeVariables = state.freeVariables; // Match up any free variables that match this scope's bindings and
      // merge then into the refs.

      for (const key of Object.keys(scope.bindings)) {
        const binding = scope.bindings[key];
        const freeVars = freeVariables.get(key);

        if (freeVars) {
          binding.refs.push(...freeVars);
          freeVariables.delete(key);
        }
      } // Move any undeclared references in this scope into the parent for
      // processing in higher scopes.


      for (const [key, value] of freeVariables) {
        let refs = parentFreeVariables.get(key);

        if (!refs) {
          refs = [];
          parentFreeVariables.set(key, refs);
        }

        refs.push(...value);
      }
    }
  }

};

function isOpeningJSXIdentifier(node, ancestors) {
  if (!t.isJSXIdentifier(node)) {
    return false;
  }

  for (let i = ancestors.length - 1; i >= 0; i--) {
    const {
      node: parent,
      key
    } = ancestors[i];

    if (t.isJSXOpeningElement(parent) && key === "name") {
      return true;
    } else if (!t.isJSXMemberExpression(parent) || key !== "object") {
      break;
    }
  }

  return false;
}

function buildMetaBindings(sourceId, node, ancestors, parentIndex = ancestors.length - 1) {
  if (parentIndex <= 1) {
    return null;
  }

  const parent = ancestors[parentIndex].node;
  const grandparent = ancestors[parentIndex - 1].node; // Consider "0, foo" to be equivalent to "foo".

  if (t.isSequenceExpression(parent) && parent.expressions.length === 2 && t.isNumericLiteral(parent.expressions[0]) && parent.expressions[1] === node) {
    let start = parent.loc.start;
    let end = parent.loc.end;

    if (t.isCallExpression(grandparent, {
      callee: parent
    })) {
      // Attempt to expand the range around parentheses, e.g.
      // (0, foo.bar)()
      start = grandparent.loc.start;
      end = Object.assign({}, end);
      end.column += 1;
    }

    return {
      type: "inherit",
      start: fromBabelLocation(start, sourceId),
      end: fromBabelLocation(end, sourceId),
      parent: buildMetaBindings(sourceId, parent, ancestors, parentIndex - 1)
    };
  } // Consider "Object(foo)" to be equivalent to "foo"


  if (t.isCallExpression(parent) && t.isIdentifier(parent.callee, {
    name: "Object"
  }) && parent.arguments.length === 1 && parent.arguments[0] === node) {
    return {
      type: "inherit",
      start: fromBabelLocation(parent.loc.start, sourceId),
      end: fromBabelLocation(parent.loc.end, sourceId),
      parent: buildMetaBindings(sourceId, parent, ancestors, parentIndex - 1)
    };
  }

  if (t.isMemberExpression(parent, {
    object: node
  })) {
    if (parent.computed) {
      if (t.isStringLiteral(parent.property)) {
        return {
          type: "member",
          start: fromBabelLocation(parent.loc.start, sourceId),
          end: fromBabelLocation(parent.loc.end, sourceId),
          property: parent.property.value,
          parent: buildMetaBindings(sourceId, parent, ancestors, parentIndex - 1)
        };
      }
    } else {
      return {
        type: "member",
        start: fromBabelLocation(parent.loc.start, sourceId),
        end: fromBabelLocation(parent.loc.end, sourceId),
        property: parent.property.name,
        parent: buildMetaBindings(sourceId, parent, ancestors, parentIndex - 1)
      };
    }
  }

  if (t.isCallExpression(parent, {
    callee: node
  }) && parent.arguments.length == 0) {
    return {
      type: "call",
      start: fromBabelLocation(parent.loc.start, sourceId),
      end: fromBabelLocation(parent.loc.end, sourceId),
      parent: buildMetaBindings(sourceId, parent, ancestors, parentIndex - 1)
    };
  }

  return null;
}

function looksLikeCommonJS(rootScope) {
  const hasRefs = name => rootScope.bindings[name] && rootScope.bindings[name].refs.length > 0;

  return hasRefs("__dirname") || hasRefs("__filename") || hasRefs("require") || hasRefs("exports") || hasRefs("module");
}

function stripModuleScope(rootScope) {
  const rootLexicalScope = rootScope.children[0];
  const moduleScope = rootLexicalScope.children[0];

  if (moduleScope.type !== "module") {
    throw new Error("Assertion failure - should be module");
  }

  Object.keys(moduleScope.bindings).forEach(name => {
    const binding = moduleScope.bindings[name];

    if (binding.type === "let" || binding.type === "const") {
      rootLexicalScope.bindings[name] = binding;
    } else {
      rootScope.bindings[name] = binding;
    }
  });
  rootLexicalScope.children = moduleScope.children;
  rootLexicalScope.children.forEach(child => {
    child.parent = rootLexicalScope;
  });
}