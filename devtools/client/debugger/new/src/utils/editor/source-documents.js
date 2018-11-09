/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { getMode } from "../source";

import { isWasm, getWasmLineNumberFormatter, renderWasmText } from "../wasm";
import { resizeBreakpointGutter, resizeToggleButton } from "../ui";
import SourceEditor from "./source-editor";

import type { Source } from "../../types";
import type { SymbolDeclarations } from "../../workers/parser";
type SourceDocuments = { [string]: Object };

let sourceDocs: SourceDocuments = {};

export function getDocument(key: string) {
  return sourceDocs[key];
}

export function hasDocument(key: string): boolean {
  return !!getDocument(key);
}

export function setDocument(key: string, doc: any) {
  sourceDocs[key] = doc;
}

export function removeDocument(key: string) {
  delete sourceDocs[key];
}

export function clearDocuments() {
  sourceDocs = {};
}

function resetLineNumberFormat(editor: SourceEditor) {
  const cm = editor.codeMirror;
  cm.setOption("lineNumberFormatter", number => number);
  resizeBreakpointGutter(cm);
  resizeToggleButton(cm);
}

export function updateLineNumberFormat(editor: SourceEditor, sourceId: string) {
  if (!isWasm(sourceId)) {
    return resetLineNumberFormat(editor);
  }
  const cm = editor.codeMirror;
  const lineNumberFormatter = getWasmLineNumberFormatter(sourceId);
  cm.setOption("lineNumberFormatter", lineNumberFormatter);
  resizeBreakpointGutter(cm);
  resizeToggleButton(cm);
}

export function updateDocument(editor: SourceEditor, source: Source) {
  if (!source) {
    return;
  }

  const sourceId = source.id;
  const doc = getDocument(sourceId) || editor.createDocument();
  editor.replaceDocument(doc);

  updateLineNumberFormat(editor, sourceId);
}

export function clearEditor(editor: SourceEditor) {
  const doc = editor.createDocument();
  editor.replaceDocument(doc);
  editor.setText("");
  editor.setMode({ name: "text" });
  resetLineNumberFormat(editor);
}

export function showLoading(editor: SourceEditor) {
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

export function showErrorMessage(editor: Object, msg: string) {
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

function setEditorText(editor: Object, source: Source) {
  if (source.isWasm) {
    const wasmLines = renderWasmText(source);
    // cm will try to split into lines anyway, saving memory
    const wasmText = { split: () => wasmLines, match: () => false };
    editor.setText(wasmText);
  } else {
    editor.setText(source.text);
  }
}

function setMode(editor, source, symbols) {
  const mode = getMode(source, symbols);
  const currentMode = editor.codeMirror.getOption("mode");
  if (!currentMode || currentMode.name != mode.name) {
    editor.setMode(mode);
  }
}

/**
 * Handle getting the source document or creating a new
 * document with the correct mode and text.
 */
export function showSourceText(
  editor: Object,
  source: Source,
  symbols?: SymbolDeclarations
) {
  if (!source) {
    return;
  }

  if (hasDocument(source.id)) {
    const doc = getDocument(source.id);
    if (editor.codeMirror.doc === doc) {
      setMode(editor, source, symbols);
      return;
    }

    editor.replaceDocument(doc);
    updateLineNumberFormat(editor, source.id);
    setMode(editor, source, symbols);
    return doc;
  }

  const doc = editor.createDocument();
  setDocument(source.id, doc);
  editor.replaceDocument(doc);

  setEditorText(editor, source);
  setMode(editor, source, symbols);
  updateLineNumberFormat(editor, source.id);
}
