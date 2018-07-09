"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.parseScript = parseScript;
exports.getAst = getAst;
exports.clearASTs = clearASTs;
exports.traverseAst = traverseAst;

var _parseScriptTags = require("parse-script-tags/index");

var _parseScriptTags2 = _interopRequireDefault(_parseScriptTags);

var _babylon = require("babylon/index");

var babylon = _interopRequireWildcard(_babylon);

var _types = require("@babel/types/index");

var t = _interopRequireWildcard(_types);

var _isEmpty = require("devtools/client/shared/vendor/lodash").isEmpty;

var _isEmpty2 = _interopRequireDefault(_isEmpty);

var _sources = require("../sources");

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
let ASTs = new Map();

function _parse(code, opts) {
  return babylon.parse(code, { ...opts,
    tokens: true
  });
}

const sourceOptions = {
  generated: {
    sourceType: "unambiguous",
    tokens: true,
    plugins: ["objectRestSpread"]
  },
  original: {
    sourceType: "unambiguous",
    tokens: true,
    plugins: ["jsx", "flow", "doExpressions", "decorators", "objectRestSpread", "classProperties", "exportDefaultFrom", "exportNamespaceFrom", "asyncGenerators", "functionBind", "functionSent", "dynamicImport", "react-jsx"]
  }
};

function parse(text, opts) {
  let ast;

  if (!text) {
    return;
  }

  try {
    ast = _parse(text, opts);
  } catch (error) {
    console.error(error);
    ast = {};
  }

  return ast;
} // Custom parser for parse-script-tags that adapts its input structure to
// our parser's signature


function htmlParser({
  source,
  line
}) {
  return parse(source, {
    startLine: line
  });
}

const VUE_COMPONENT_START = /^\s*</;

function vueParser({
  source,
  line
}) {
  return parse(source, {
    startLine: line,
    ...sourceOptions.original
  });
}

function parseVueScript(code) {
  if (typeof code !== "string") {
    return;
  }

  let ast; // .vue files go through several passes, so while there is a
  // single-file-component Vue template, there are also generally .vue files
  // that are still just JS as well.

  if (code.match(VUE_COMPONENT_START)) {
    ast = (0, _parseScriptTags2.default)(code, vueParser);

    if (t.isFile(ast)) {
      // parseScriptTags is currently hard-coded to return scripts, but Vue
      // always expects ESM syntax, so we just hard-code it.
      ast.program.sourceType = "module";
    }
  } else {
    ast = parse(code, sourceOptions.original);
  }

  return ast;
}

function parseScript(text, opts) {
  return _parse(text, opts);
}

function getAst(sourceId) {
  if (ASTs.has(sourceId)) {
    return ASTs.get(sourceId);
  }

  const source = (0, _sources.getSource)(sourceId);
  let ast = {};
  const {
    contentType
  } = source;

  if (contentType == "text/html") {
    ast = (0, _parseScriptTags2.default)(source.text, htmlParser) || {};
  } else if (contentType && contentType === "text/vue") {
    ast = parseVueScript(source.text) || {};
  } else if (contentType && contentType.match(/(javascript|jsx)/) && !contentType.match(/typescript-jsx/)) {
    const type = source.id.includes("original") ? "original" : "generated";
    const options = sourceOptions[type];
    ast = parse(source.text, options);
  } else if (contentType && contentType.match(/typescript/)) {
    const options = { ...sourceOptions.original,
      plugins: [...sourceOptions.original.plugins.filter(p => p !== "flow" && p !== "decorators" && p !== "decorators2" && (p !== "jsx" || contentType.match(/typescript-jsx/))), "decorators", "typescript"]
    };
    ast = parse(source.text, options);
  }

  ASTs.set(source.id, ast);
  return ast;
}

function clearASTs() {
  ASTs = new Map();
}

function traverseAst(sourceId, visitor, state) {
  const ast = getAst(sourceId);

  if ((0, _isEmpty2.default)(ast)) {
    return null;
  }

  t.traverse(ast, visitor, state);
  return ast;
}