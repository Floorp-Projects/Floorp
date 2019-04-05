/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global __dirname */
const path = require("path");

module.exports = [{
  bail: true,
  entry: [
    "./codemirror/addon/dialog/dialog.js",
    "./codemirror/addon/search/searchcursor.js",
    "./codemirror/addon/search/search.js",
    "./codemirror/addon/edit/matchbrackets.js",
    "./codemirror/addon/edit/closebrackets.js",
    "./codemirror/addon/comment/comment.js",
    "./codemirror/addon/accessibleTextarea.js",
    "./codemirror/mode/javascript/javascript.js",
    "./codemirror/mode/xml/xml.js",
    "./codemirror/mode/css/css.js",
    "./codemirror/mode/haxe/haxe.js",
    "./codemirror/mode/htmlmixed/htmlmixed.js",
    "./codemirror/mode/jsx/jsx.js",
    "./codemirror/mode/coffeescript/coffeescript.js",
    "./codemirror/mode/elm/elm.js",
    "./codemirror/mode/clike/clike.js",
    "./codemirror/mode/wasm/wasm.js",
    "./codemirror/addon/selection/active-line.js",
    "./codemirror/addon/edit/trailingspace.js",
    "./codemirror/addon/fold/foldcode.js",
    "./codemirror/addon/fold/brace-fold.js",
    "./codemirror/addon/fold/comment-fold.js",
    "./codemirror/addon/fold/xml-fold.js",
    "./codemirror/addon/fold/foldgutter.js",
    "./codemirror/addon/runmode/runmode.js",
    "./codemirror/lib/codemirror.js",
  ],
  output: {
    path: path.resolve(__dirname, "./codemirror/"),
    filename: "codemirror.bundle.js",
    libraryTarget: "var",
    library: "CodeMirror",
  },
}];
