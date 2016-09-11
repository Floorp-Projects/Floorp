/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.exports = [{
  bail: true,
  entry: [
    "./sourceeditor/codemirror/addon/dialog/dialog.js",
    "./sourceeditor/codemirror/addon/search/searchcursor.js",
    "./sourceeditor/codemirror/addon/search/search.js",
    "./sourceeditor/codemirror/addon/edit/matchbrackets.js",
    "./sourceeditor/codemirror/addon/edit/closebrackets.js",
    "./sourceeditor/codemirror/addon/comment/comment.js",
    "./sourceeditor/codemirror/mode/javascript/javascript.js",
    "./sourceeditor/codemirror/mode/xml/xml.js",
    "./sourceeditor/codemirror/mode/css/css.js",
    "./sourceeditor/codemirror/mode/htmlmixed/htmlmixed.js",
    "./sourceeditor/codemirror/mode/clike/clike.js",
    "./sourceeditor/codemirror/mode/wasm/wasm.js",
    "./sourceeditor/codemirror/addon/selection/active-line.js",
    "./sourceeditor/codemirror/addon/edit/trailingspace.js",
    "./sourceeditor/codemirror/keymap/emacs.js",
    "./sourceeditor/codemirror/keymap/vim.js",
    "./sourceeditor/codemirror/keymap/sublime.js",
    "./sourceeditor/codemirror/addon/fold/foldcode.js",
    "./sourceeditor/codemirror/addon/fold/brace-fold.js",
    "./sourceeditor/codemirror/addon/fold/comment-fold.js",
    "./sourceeditor/codemirror/addon/fold/xml-fold.js",
    "./sourceeditor/codemirror/addon/fold/foldgutter.js",
    "./sourceeditor/codemirror/lib/codemirror.js",
  ],
  output: {
    filename: "./sourceeditor/codemirror/codemirror.bundle.js",
    libraryTarget: "var",
    library: "CodeMirror",
  },
}];
