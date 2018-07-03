"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getDocument = getDocument;
exports.hasDocument = hasDocument;
exports.setDocument = setDocument;
exports.removeDocument = removeDocument;
exports.clearDocuments = clearDocuments;
exports.updateLineNumberFormat = updateLineNumberFormat;
exports.updateDocument = updateDocument;
exports.clearEditor = clearEditor;
exports.showLoading = showLoading;
exports.showErrorMessage = showErrorMessage;
exports.showSourceText = showSourceText;

var _source = require("../source");

var _wasm = require("../wasm");

var _ui = require("../ui");

var _sourceEditor = require("devtools/client/sourceeditor/editor");

var _sourceEditor2 = _interopRequireDefault(_sourceEditor);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
let sourceDocs = {};

function getDocument(key) {
  return sourceDocs[key];
}

function hasDocument(key) {
  return !!getDocument(key);
}

function setDocument(key, doc) {
  sourceDocs[key] = doc;
}

function removeDocument(key) {
  delete sourceDocs[key];
}

function clearDocuments() {
  sourceDocs = {};
}

function resetLineNumberFormat(editor) {
  const cm = editor.codeMirror;
  cm.setOption("lineNumberFormatter", number => number);
  (0, _ui.resizeBreakpointGutter)(cm);
  (0, _ui.resizeToggleButton)(cm);
}

function updateLineNumberFormat(editor, sourceId) {
  if (!(0, _wasm.isWasm)(sourceId)) {
    return resetLineNumberFormat(editor);
  }

  const cm = editor.codeMirror;
  const lineNumberFormatter = (0, _wasm.getWasmLineNumberFormatter)(sourceId);
  cm.setOption("lineNumberFormatter", lineNumberFormatter);
  (0, _ui.resizeBreakpointGutter)(cm);
  (0, _ui.resizeToggleButton)(cm);
}

function updateDocument(editor, source) {
  if (!source) {
    return;
  }

  const sourceId = source.id;
  const doc = getDocument(sourceId) || editor.createDocument();
  editor.replaceDocument(doc);
  updateLineNumberFormat(editor, sourceId);
}

function clearEditor(editor) {
  const doc = editor.createDocument();
  editor.replaceDocument(doc);
  editor.setText("");
  editor.setMode({
    name: "text"
  });
  resetLineNumberFormat(editor);
}

function showLoading(editor) {
  if (hasDocument("loading")) {
    return;
  }

  const doc = editor.createDocument();
  setDocument("loading", doc);
  editor.replaceDocument(doc);
  editor.setText(L10N.getStr("loadingText"));
  editor.setMode({
    name: "text"
  });
}

function showErrorMessage(editor, msg) {
  let error;

  if (msg.includes("WebAssembly binary source is not available")) {
    error = L10N.getStr("wasmIsNotAvailable");
  } else {
    error = L10N.getFormatStr("errorLoadingText3", msg);
  }

  const doc = editor.createDocument();
  editor.replaceDocument(doc);
  editor.setText(error);
  editor.setMode({
    name: "text"
  });
  resetLineNumberFormat(editor);
}

function setEditorText(editor, source) {
  const {
    text,
    id: sourceId
  } = source;

  if (source.isWasm) {
    const wasmLines = (0, _wasm.renderWasmText)(sourceId, text); // cm will try to split into lines anyway, saving memory

    const wasmText = {
      split: () => wasmLines,
      match: () => false
    };
    editor.setText(wasmText);
  } else {
    editor.setText(text);
  }
}
/**
 * Handle getting the source document or creating a new
 * document with the correct mode and text.
 */


function showSourceText(editor, source, symbols) {
  if (!source) {
    return;
  }

  if (hasDocument(source.id)) {
    const doc = getDocument(source.id);

    if (editor.codeMirror.doc === doc) {
      const mode = (0, _source.getMode)(source, symbols);

      if (doc.mode.name !== mode.name) {
        editor.setMode(mode);
      }

      return;
    }

    editor.replaceDocument(doc);
    updateLineNumberFormat(editor, source.id);
    editor.setMode((0, _source.getMode)(source, symbols));
    return doc;
  }

  const doc = editor.createDocument();
  setDocument(source.id, doc);
  editor.replaceDocument(doc);
  setEditorText(editor, source);
  editor.setMode((0, _source.getMode)(source, symbols));
  updateLineNumberFormat(editor, source.id);
}