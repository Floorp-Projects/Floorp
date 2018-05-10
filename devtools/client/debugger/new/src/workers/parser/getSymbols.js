"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.clearSymbols = clearSymbols;
exports.getSymbols = getSymbols;

var _types = require("@babel/types/index");

var t = _interopRequireWildcard(_types);

var _simplePath = require("./utils/simple-path");

var _simplePath2 = _interopRequireDefault(_simplePath);

var _ast = require("./utils/ast");

var _helpers = require("./utils/helpers");

var _inferClassName = require("./utils/inferClassName");

var _getFunctionName = require("./utils/getFunctionName");

var _getFunctionName2 = _interopRequireDefault(_getFunctionName);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

let symbolDeclarations = new Map();

function getFunctionParameterNames(path) {
  if (path.node.params != null) {
    return path.node.params.map(param => {
      if (param.type !== "AssignmentPattern") {
        return param.name;
      } // Parameter with default value


      if (param.left.type === "Identifier" && param.right.type === "Identifier") {
        return `${param.left.name} = ${param.right.name}`;
      } else if (param.left.type === "Identifier" && param.right.type === "StringLiteral") {
        return `${param.left.name} = ${param.right.value}`;
      } else if (param.left.type === "Identifier" && param.right.type === "ObjectExpression") {
        return `${param.left.name} = {}`;
      } else if (param.left.type === "Identifier" && param.right.type === "ArrayExpression") {
        return `${param.left.name} = []`;
      } else if (param.left.type === "Identifier" && param.right.type === "NullLiteral") {
        return `${param.left.name} = null`;
      }
    });
  }

  return [];
}
/* eslint-disable complexity */


function extractSymbol(path, symbols) {
  if ((0, _helpers.isVariable)(path)) {
    symbols.variables.push(...(0, _helpers.getVariableNames)(path));
  }

  if ((0, _helpers.isFunction)(path)) {
    symbols.functions.push({
      name: (0, _getFunctionName2.default)(path.node, path.parent),
      klass: (0, _inferClassName.inferClassName)(path),
      location: path.node.loc,
      parameterNames: getFunctionParameterNames(path),
      identifier: path.node.id
    });
  }

  if (t.isJSXElement(path)) {
    symbols.hasJsx = true;
  }

  if (t.isGenericTypeAnnotation(path)) {
    symbols.hasTypes = true;
  }

  if (t.isClassDeclaration(path)) {
    symbols.classes.push({
      name: path.node.id.name,
      parent: path.node.superClass,
      location: path.node.loc
    });
  }

  if (t.isImportDeclaration(path)) {
    symbols.imports.push({
      source: path.node.source.value,
      location: path.node.loc,
      specifiers: (0, _helpers.getSpecifiers)(path.node.specifiers)
    });
  }

  if (t.isObjectProperty(path)) {
    const {
      start,
      end,
      identifierName
    } = path.node.key.loc;
    symbols.objectProperties.push({
      name: identifierName,
      location: {
        start,
        end
      },
      expression: getSnippet(path)
    });
  }

  if (t.isMemberExpression(path)) {
    const {
      start,
      end
    } = path.node.property.loc;
    symbols.memberExpressions.push({
      name: path.node.property.name,
      location: {
        start,
        end
      },
      expression: getSnippet(path),
      computed: path.node.computed
    });
  }

  if ((t.isStringLiteral(path) || t.isNumericLiteral(path)) && t.isMemberExpression(path.parentPath)) {
    // We only need literals that are part of computed memeber expressions
    const {
      start,
      end
    } = path.node.loc;
    symbols.literals.push({
      name: path.node.value,
      location: {
        start,
        end
      },
      expression: getSnippet(path.parentPath)
    });
  }

  if (t.isCallExpression(path)) {
    const callee = path.node.callee;
    const args = path.node.arguments;

    if (!t.isMemberExpression(callee)) {
      const {
        start,
        end,
        identifierName
      } = callee.loc;
      symbols.callExpressions.push({
        name: identifierName,
        values: args.filter(arg => arg.value).map(arg => arg.value),
        location: {
          start,
          end
        }
      });
    }
  }

  if (t.isStringLiteral(path) && t.isProperty(path.parentPath)) {
    const {
      start,
      end
    } = path.node.loc;
    return symbols.identifiers.push({
      name: path.node.value,
      expression: (0, _helpers.getObjectExpressionValue)(path.parent),
      location: {
        start,
        end
      }
    });
  }

  if (t.isIdentifier(path) && !t.isGenericTypeAnnotation(path.parent)) {
    let {
      start,
      end
    } = path.node.loc; // We want to include function params, but exclude the function name

    if (t.isClassMethod(path.parent) && !path.inList) {
      return;
    }

    if (t.isProperty(path.parentPath) && !(0, _helpers.isObjectShorthand)(path.parent)) {
      return symbols.identifiers.push({
        name: path.node.name,
        expression: (0, _helpers.getObjectExpressionValue)(path.parent),
        location: {
          start,
          end
        }
      });
    }

    if (path.node.typeAnnotation) {
      const column = path.node.typeAnnotation.loc.start.column;
      end = _objectSpread({}, end, {
        column
      });
    }

    symbols.identifiers.push({
      name: path.node.name,
      expression: path.node.name,
      location: {
        start,
        end
      }
    });
  }

  if (t.isThisExpression(path.node)) {
    const {
      start,
      end
    } = path.node.loc;
    symbols.identifiers.push({
      name: "this",
      location: {
        start,
        end
      },
      expression: "this"
    });
  }

  if (t.isVariableDeclarator(path)) {
    const nodeId = path.node.id;

    if (t.isArrayPattern(nodeId)) {
      return;
    }

    const properties = nodeId.properties && t.objectPattern(nodeId.properties) ? nodeId.properties : [{
      value: {
        name: nodeId.name
      },
      loc: path.node.loc
    }];
    properties.forEach(function (property) {
      const {
        start,
        end
      } = property.loc;
      symbols.identifiers.push({
        name: property.value.name,
        expression: property.value.name,
        location: {
          start,
          end
        }
      });
    });
  }
}
/* eslint-enable complexity */


function extractSymbols(sourceId) {
  const symbols = {
    functions: [],
    variables: [],
    callExpressions: [],
    memberExpressions: [],
    objectProperties: [],
    comments: [],
    identifiers: [],
    classes: [],
    imports: [],
    literals: [],
    hasJsx: false,
    hasTypes: false
  };
  const ast = (0, _ast.traverseAst)(sourceId, {
    enter(node, ancestors) {
      try {
        const path = (0, _simplePath2.default)(ancestors);

        if (path) {
          extractSymbol(path, symbols);
        }
      } catch (e) {
        console.error(e);
      }
    }

  }); // comments are extracted separately from the AST

  symbols.comments = (0, _helpers.getComments)(ast);
  return symbols;
}

function extendSnippet(name, expression, path, prevPath) {
  const computed = path && path.node.computed;
  const prevComputed = prevPath && prevPath.node.computed;
  const prevArray = t.isArrayExpression(prevPath);
  const array = t.isArrayExpression(path);
  const value = path && path.node.property && path.node.property.extra && path.node.property.extra.raw || "";

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

  if ((0, _helpers.isComputedExpression)(expression) && name !== undefined) {
    return `${name}${expression}`;
  }

  return `${name}.${expression}`;
}

function getMemberSnippet(node, expression = "") {
  if (t.isMemberExpression(node)) {
    const name = node.property.name;
    const snippet = getMemberSnippet(node.object, extendSnippet(name, expression, {
      node
    }));
    return snippet;
  }

  if (t.isCallExpression(node)) {
    return "";
  }

  if (t.isThisExpression(node)) {
    return `this.${expression}`;
  }

  if (t.isIdentifier(node)) {
    if ((0, _helpers.isComputedExpression)(expression)) {
      return `${node.name}${expression}`;
    }

    return `${node.name}.${expression}`;
  }

  return expression;
}

function getObjectSnippet(path, prevPath, expression = "") {
  if (!path) {
    return expression;
  }

  const name = path.node.key.name;
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
    const name = node.id.name;
    return extendSnippet(name, expression, path, prevPath);
  }

  if (t.isVariableDeclarator(path)) {
    const node = path.node.id;

    if (t.isObjectPattern(node)) {
      return expression;
    }

    const name = node.name;
    const prop = extendSnippet(name, expression, path, prevPath);
    return prop;
  }

  if (t.isAssignmentExpression(path)) {
    const node = path.node.left;
    const name = t.isMemberExpression(node) ? getMemberSnippet(node) : node.name;
    const prop = extendSnippet(name, expression, path, prevPath);
    return prop;
  }

  if ((0, _helpers.isFunction)(path)) {
    return expression;
  }

  if (t.isIdentifier(path)) {
    const node = path.node;
    return `${node.name}.${expression}`;
  }

  if (t.isObjectProperty(path)) {
    return getObjectSnippet(path, prevPath, expression);
  }

  if (t.isObjectExpression(path)) {
    const parentPath = prevPath && prevPath.parentPath;
    return getObjectSnippet(parentPath, prevPath, expression);
  }

  if (t.isMemberExpression(path)) {
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

function clearSymbols() {
  symbolDeclarations = new Map();
}

function getSymbols(sourceId) {
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