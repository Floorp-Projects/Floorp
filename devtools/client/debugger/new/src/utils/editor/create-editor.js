"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.createEditor = createEditor;

var _sourceEditor = require("devtools/client/sourceeditor/editor");

var _sourceEditor2 = _interopRequireDefault(_sourceEditor);

var _prefs = require("../prefs");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function createEditor() {
  const gutters = ["breakpoints", "hit-markers", "CodeMirror-linenumbers"];

  if (_prefs.features.codeFolding) {
    gutters.push("CodeMirror-foldgutter");
  }

  return new _sourceEditor2.default({
    mode: "javascript",
    foldGutter: _prefs.features.codeFolding,
    enableCodeFolding: _prefs.features.codeFolding,
    readOnly: true,
    lineNumbers: true,
    theme: "mozilla",
    styleActiveLine: false,
    lineWrapping: false,
    matchBrackets: true,
    showAnnotationRuler: true,
    gutters,
    value: " ",
    extraKeys: {
      // Override code mirror keymap to avoid conflicts with split console.
      Esc: false,
      "Cmd-F": false,
      "Ctrl-F": false,
      "Cmd-G": false,
      "Ctrl-G": false
    }
  });
}