/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { isWasm, getWasmLineNumberFormatter, renderWasmText } from "../wasm";
import { isMinified } from "../isMinified";
import { resizeBreakpointGutter, resizeToggleButton } from "../ui";
import { javascriptLikeExtensions } from "../source";

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

function updateLineNumberFormat(editor, sourceId) {
  if (!isWasm(sourceId)) {
    resetLineNumberFormat(editor);
    return;
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
  const doc = editor.createDocument("", { name: "text" });
  editor.replaceDocument(doc);
  resetLineNumberFormat(editor);
}

export function showLoading(editor) {
  let doc = getDocument("loading");

  if (doc) {
    editor.replaceDocument(doc);
  } else {
    doc = editor.createDocument(L10N.getStr("loadingText"), { name: "text" });
    setDocument("loading", doc);
  }
}

export function showErrorMessage(editor, msg) {
  let error;
  if (msg.includes("WebAssembly binary source is not available")) {
    error = L10N.getStr("wasmIsNotAvailable");
  } else {
    error = L10N.getFormatStr("errorLoadingText3", msg);
  }
  const doc = editor.createDocument(error, { name: "text" });
  editor.replaceDocument(doc);
  resetLineNumberFormat(editor);
}

const contentTypeModeMap = new Map([
  ["text/javascript", { name: "javascript" }],
  ["text/typescript", { name: "javascript", typescript: true }],
  ["text/coffeescript", { name: "coffeescript" }],
  [
    "text/typescript-jsx",
    {
      name: "jsx",
      base: { name: "javascript", typescript: true },
    },
  ],
  ["text/jsx", { name: "jsx" }],
  ["text/x-elm", { name: "elm" }],
  ["text/x-clojure", { name: "clojure" }],
  ["text/x-clojurescript", { name: "clojure" }],
  ["text/wasm", { name: "text" }],
  ["text/html", { name: "htmlmixed" }],
  ["text/plain", { name: "text" }],
]);

const nonJSLanguageExtensionMap = new Map([
  ["c", { name: "text/x-csrc" }],
  ["kt", { name: "text/x-kotlin" }],
  ["cpp", { name: "text/x-c++src" }],
  ["m", { name: "text/x-objectivec" }],
  ["rs", { name: "text/x-rustsrc" }],
  ["hx", { name: "text/x-haxe" }],
]);

/**
 * Returns Code Mirror mode for source content type
 */
// eslint-disable-next-line complexity
export function getMode(source, sourceTextContent, symbols) {
  const content = sourceTextContent.value;
  // Disable modes for minified files with 1+ million characters (See Bug 1569829).
  if (
    content.type === "text" &&
    isMinified(source, sourceTextContent) &&
    content.value.length > 1000000
  ) {
    return contentTypeModeMap.get("text/plain");
  }

  if (content.type !== "text") {
    return contentTypeModeMap.get("text/plain");
  }

  const extension = source.displayURL.fileExtension;
  if (extension === "jsx" || (symbols && symbols.hasJsx)) {
    if (symbols && symbols.hasTypes) {
      return contentTypeModeMap.get("text/typescript-jsx");
    }
    return contentTypeModeMap.get("text/jsx");
  }

  if (symbols && symbols.hasTypes) {
    if (symbols.hasJsx) {
      return contentTypeModeMap.get("text/typescript-jsx");
    }

    return contentTypeModeMap.get("text/typescript");
  }

  // check for C and other non JS languages
  if (nonJSLanguageExtensionMap.has(extension)) {
    return nonJSLanguageExtensionMap.get(extension);
  }

  // if the url ends with a known Javascript-like URL, provide JavaScript mode.
  if (javascriptLikeExtensions.has(extension)) {
    return contentTypeModeMap.get("text/javascript");
  }

  const { contentType, value: text } = content;
  // Use HTML mode for files in which the first non whitespace
  // character is `<` regardless of extension.
  const isHTMLLike = () => text.match(/^\s*</);
  if (!contentType) {
    if (isHTMLLike()) {
      return contentTypeModeMap.get("text/html");
    }
    return contentTypeModeMap.get("text/plain");
  }

  // // @flow or /* @flow */
  if (text.match(/^\s*(\/\/ @flow|\/\* @flow \*\/)/)) {
    return contentTypeModeMap.get("text/typescript");
  }

  if (contentTypeModeMap.has(contentType)) {
    return contentTypeModeMap.get(contentType);
  }

  if (isHTMLLike()) {
    return contentTypeModeMap.get("text/html");
  }

  return contentTypeModeMap.get("text/plain");
}

function setMode(editor, source, sourceTextContent, symbols) {
  const mode = getMode(source, sourceTextContent, symbols);
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
    return;
  }

  const content = sourceTextContent.value;

  const doc = editor.createDocument(
    // We can set wasm text content directly from the constructor, so we pass an empty string
    // here, and set the text after replacing the document.
    content.type !== "wasm" ? content.value : "",
    getMode(source, sourceTextContent, symbols)
  );

  setDocument(source.id, doc);
  editor.replaceDocument(doc);

  if (content.type === "wasm") {
    const wasmLines = renderWasmText(source.id, content);
    // cm will try to split into lines anyway, saving memory
    editor.setText({ split: () => wasmLines, match: () => false });
  }

  updateLineNumberFormat(editor, source.id);
}
