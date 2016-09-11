/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// WebAssembly experimental syntax highlight add-on for CodeMirror.

(function (root, factory) {
  if (typeof define === 'function' && define.amd) {
    define(["../../lib/codemirror"], factory);
  } else if (typeof exports !== 'undefined') {
    factory(require("../../lib/codemirror"));
  } else {
    factory(root.CodeMirror);
  }
}(this, function (CodeMirror) {
"use strict";

var isWordChar = /[\w\$_\.\\\/@]/;

function createLookupTable(list) {
  var obj = Object.create(null);
  list.forEach(function (key) {
    obj[key] = true;
  });
  return obj;
}

CodeMirror.defineMode("wasm", function() {
  var keywords = createLookupTable([
    "function", "import", "export", "table", "memory", "segment", "as", "type",
    "of", "from", "typeof", "br", "br_if", "loop", "br_table", "if", "else",
    "call", "call_import", "call_indirect", "nop", "unreachable", "var",
    "align", "select", "return"]);
  var builtins = createLookupTable([
    "i32:8", "i32:8u", "i32:8s", "i32:16", "i32:16u", "i32:16s",
    "i64:8", "i64:8u", "i64:8s", "i64:16", "i64:16u", "i64:16s",
    "i64:32", "i64:32u", "i64:32s",
    "i32.add", "i32.sub", "i32.mul", "i32.div_s", "i32.div_u",
    "i32.rem_s", "i32.rem_u", "i32.and", "i32.or", "i32.xor",
    "i32.shl", "i32.shr_u", "i32.shr_s", "i32.rotr", "i32.rotl",
    "i32.eq", "i32.ne", "i32.lt_s", "i32.le_s", "i32.lt_u",
    "i32.le_u", "i32.gt_s", "i32.ge_s", "i32.gt_u", "i32.ge_u",
    "i32.clz", "i32.ctz", "i32.popcnt", "i32.eqz", "i64.add",
    "i64.sub", "i64.mul", "i64.div_s", "i64.div_u", "i64.rem_s",
    "i64.rem_u", "i64.and", "i64.or", "i64.xor", "i64.shl",
    "i64.shr_u", "i64.shr_s", "i64.rotr", "i64.rotl", "i64.eq",
    "i64.ne", "i64.lt_s", "i64.le_s", "i64.lt_u", "i64.le_u",
    "i64.gt_s", "i64.ge_s", "i64.gt_u", "i64.ge_u", "i64.clz",
    "i64.ctz", "i64.popcnt", "i64.eqz", "f32.add", "f32.sub",
    "f32.mul", "f32.div", "f32.min", "f32.max", "f32.abs",
    "f32.neg", "f32.copysign", "f32.ceil", "f32.floor", "f32.trunc",
    "f32.nearest", "f32.sqrt", "f32.eq", "f32.ne", "f32.lt",
    "f32.le", "f32.gt", "f32.ge", "f64.add", "f64.sub", "f64.mul",
    "f64.div", "f64.min", "f64.max", "f64.abs", "f64.neg",
    "f64.copysign", "f64.ceil", "f64.floor", "f64.trunc", "f64.nearest",
    "f64.sqrt", "f64.eq", "f64.ne", "f64.lt", "f64.le", "f64.gt",
    "f64.ge", "i32.trunc_s/f32", "i32.trunc_s/f64", "i32.trunc_u/f32",
    "i32.trunc_u/f64", "i32.wrap/i64", "i64.trunc_s/f32",
    "i64.trunc_s/f64", "i64.trunc_u/f32", "i64.trunc_u/f64",
    "i64.extend_s/i32", "i64.extend_u/i32", "f32.convert_s/i32",
    "f32.convert_u/i32", "f32.convert_s/i64", "f32.convert_u/i64",
    "f32.demote/f64", "f32.reinterpret/i32", "f64.convert_s/i32",
    "f64.convert_u/i32", "f64.convert_s/i64", "f64.convert_u/i64",
    "f64.promote/f32", "f64.reinterpret/i64", "i32.reinterpret/f32",
    "i64.reinterpret/f64"]);
  var dataTypes = createLookupTable(["i32", "i64", "f32", "f64"]);
  var isUnaryOperator = /[\-!]/;
  var operators = createLookupTable([
    "+", "-", "*", "/", "/s", "/u", "%", "%s", "%u",
    "<<", ">>u", ">>s", ">=", "<=", "==", "!=",
    "<s", "<u", "<=s", "<=u", ">=s", ">=u", ">s", ">u",
    "<", ">", "=", "&", "|", "^", "!"]);

  function tokenBase(stream, state) {
    var ch = stream.next();
    if (ch === "$") {
      stream.eatWhile(isWordChar);
      return "variable";
    }
    if (ch === "@") {
      stream.eatWhile(isWordChar);
      return "meta";
    }
    if (ch === '"') {
      state.tokenize = tokenString(ch);
      return state.tokenize(stream, state);
    }
    if (ch == "/") {
      if (stream.eat("*")) {
        state.tokenize = tokenComment;
        return tokenComment(stream, state);
      } else if (stream.eat("/")) {
        stream.skipToEnd();
        return "comment";
      }
    }
    if (/\d/.test(ch) ||
        ((ch === "-" || ch === "+") && /\d/.test(stream.peek()))) {
      stream.eatWhile(/[\w\._\-+]/);
      return "number";
    }
    if (/[\[\]\(\)\{\},:]/.test(ch)) {
      return null;
    }
    if (isUnaryOperator.test(ch)) {
      return "operator";
    }
    stream.eatWhile(isWordChar);
    var word = stream.current();

    if (word in operators) {
      return "operator";
    }
    if (word in keywords){
      return "keyword";
    }
    if (word in dataTypes) {
      if (!stream.eat(":")) {
        return "builtin";
      }
      stream.eatWhile(isWordChar);
      word = stream.current();
      // fall thru for "builtin" check
    }
    if (word in builtins) {
      return "builtin";
    }

    if (word === "Temporary") {
      // Nightly has header with some text graphics -- skipping it.
      state.tokenize = tokenTemporary;
      return state.tokenize(stream, state);
    }
    return null;
  }

  function tokenComment(stream, state) {
    state.commentDepth = 1;
    var next;
    while ((next = stream.next()) != null) {
      if (next === "*" && stream.eat("/")) {
        if (--state.commentDepth === 0) {
          state.tokenize = null;
          return "comment";
        }
      }
      if (next === "/" && stream.eat("*")) {
        // Nested comment
        state.commentDepth++;
      }
    }
    return "comment";
  }

  function tokenTemporary(stream, state) {
    var next, endState = state.commentState;
    // Skipping until "text support (Work In Progress):" is found.
    while ((next = stream.next()) != null) {
      if (endState === 0 && next === "t") {
        endState = 1;
      } else if (endState === 1 && next === ":") {
        state.tokenize = null;
        state.commentState = 0;
        endState = 2;
        return "comment";
      }
    }
    state.commentState = endState;
    return "comment";
  }

  function tokenString(quote) {
    return function(stream, state) {
      var escaped = false, next, end = false;
      while ((next = stream.next()) != null) {
        if (next == quote && !escaped) {
          state.tokenize = null;
          return "string";
        }
        escaped = !escaped && next === "\\";
      }
      return "string";
    };
  }

  return {
    startState: function() {
      return {tokenize: null, commentState: 0, commentDepth: 0};
    },

    token: function(stream, state) {
      if (stream.eatSpace()) return null;
      var style = (state.tokenize || tokenBase)(stream, state);
      return style;
    }
  };
});

CodeMirror.registerHelper("wordChars", "wasm", isWordChar);

CodeMirror.defineMIME("text/wasm", "wasm");

}));
