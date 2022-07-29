/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { getMode } from "../source";

import { isWasm, getWasmLineNumberFormatter, renderWasmText } from "../wasm";
import { isMinified } from "../isMinified";
import { resizeBreakpointGutter, resizeToggleButton } from "../ui";

let sourceDocs = {};

export function getDocument(key) {
  return sourceDocs[key];
}

export function hasDocument(key) {
  return !!getDocument(key);
}

export function setDocument(key, doc) {
  sourceDocs[key] = doc;
}

export function removeDocument(key) {
  delete sourceDocs[key];
}

export function clearDocuments() {
  sourceDocs = {};
}

function resetLineNumberFormat(editor) {
  const cm = editor.codeMirror;
  cm.setOption("lineNumberFormatter", number => number);
  resizeBreakpointGutter(cm);
  resizeToggleButton(cm);
}

export function updateLineNumberFormat(editor, sourceId) {
  if (!isWasm(sourceId)) {
    return resetLineNumberFormat(editor);
  }
  const cm = editor.codeMirror;
  const lineNumberFormatter = getWasmLineNumberFormatter(sourceId);
  cm.setOption("lineNumberFormatter", lineNumberFormatter);
  resizeBreakpointGutter(cm);
  resizeToggleButton(cm);
}

export function updateDocument(editor, source) {
  if (!source) {
    return;
  }

  const sourceId = source.id;
  const doc = getDocument(sourceId) || editor.createDocument();
  editor.replaceDocument(doc);

  updateLineNumberFormat(editor, sourceId);
}

/* used to apply the context menu wrap line option change to all the docs */
export function updateDocuments(updater) {
  for (const key in sourceDocs) {
    if (sourceDocs[key].cm == null) {
      continue;
    } else {
      updater(sourceDocs[key]);
    }
  }
}

export function clearEditor(editor) {
  const doc = editor.createDocument();
  editor.replaceDocument(doc);
  editor.setText("");
  editor.setMode({ name: "text" });
  resetLineNumberFormat(editor);
}

export function showLoading(editor) {
  let doc = getDocument("loading");

  if (doc) {
    editor.replaceDocument(doc);
  } else {
    doc = editor.createDocument();
    setDocument("loading", doc);
    doc.setValue(L10N.getStr("loadingText"));
    editor.replaceDocument(doc);
    editor.setMode({ name: "text" });
  }
}

export function showErrorMessage(editor, msg) {
  let error;
  if (msg.includes("WebAssembly binary source is not available")) {
    error = L10N.getStr("wasmIsNotAvailable");
  } else {
    error = L10N.getFormatStr("errorLoadingText3", msg);
  }
  const doc = editor.createDocument();
  editor.replaceDocument(doc);
  editor.setText(error);
  editor.setMode({ name: "text" });
  resetLineNumberFormat(editor);
}

function setEditorText(editor, sourceId, content) {
  if (content.type === "wasm") {
    const wasmLines = renderWasmText(sourceId, content);
    // cm will try to split into lines anyway, saving memory
    const wasmText = { split: () => wasmLines, match: () => false };
    editor.setText(wasmText);
  } else {
    editor.setText(content.value);
  }
}

function setMode(editor, source, sourceTextContent, symbols) {
  // Disable modes for minified files with 1+ million characters Bug 1569829
  const content = sourceTextContent.value;
  if (
    content.type === "text" &&
    isMinified(source, sourceTextContent) &&
    content.value.length > 1000000
  ) {
    return;
  }

  const mode = getMode(source, content, symbols);
  const currentMode = editor.codeMirror.getOption("mode");
  if (!currentMode || currentMode.name != mode.name) {
    editor.setMode(mode);
  }
}

/**
 * Handle getting the source document or creating a new
 * document with the correct mode and text.
 */
export function showSourceText(editor, source, sourceTextContent, symbols) {
  if (hasDocument(source.id)) {
    const doc = getDocument(source.id);
    if (editor.codeMirror.doc === doc) {
      setMode(editor, source, sourceTextContent, symbols);
      return;
    }

    editor.replaceDocument(doc);
    updateLineNumberFormat(editor, source.id);
    setMode(editor, source, sourceTextContent, symbols);
    return doc;
  }

  const doc = editor.createDocument();
  setDocument(source.id, doc);
  editor.replaceDocument(doc);

  setEditorText(editor, source.id, sourceTextContent.value);
  setMode(editor, source, sourceTextContent, symbols);
  updateLineNumberFormat(editor, source.id);
}
