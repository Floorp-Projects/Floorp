"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.createEditor = createEditor;

var _sourceEditor = require("devtools/client/sourceeditor/editor");

var _sourceEditor2 = _interopRequireDefault(_sourceEditor);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function createEditor(value) {
  return new _sourceEditor2.default({
    mode: "javascript",
    foldGutter: false,
    enableCodeFolding: false,
    readOnly: "nocursor",
    lineNumbers: false,
    theme: "mozilla mozilla-breakpoint",
    styleActiveLine: false,
    lineWrapping: false,
    matchBrackets: false,
    showAnnotationRuler: false,
    gutters: false,
    value: value || "",
    scrollbarStyle: null
  });
}