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

const contentTypeModeMap = {
  "text/javascript": { name: "javascript" },
  "text/typescript": { name: "javascript", typescript: true },
  "text/coffeescript": { name: "coffeescript" },
  "text/typescript-jsx": {
    name: "jsx",
    base: { name: "javascript", typescript: true },
  },
  "text/jsx": { name: "jsx" },
  "text/x-elm": { name: "elm" },
  "text/x-clojure": { name: "clojure" },
  "text/x-clojurescript": { name: "clojure" },
  "text/wasm": { name: "text" },
  "text/html": { name: "htmlmixed" },
};

const languageMimeMap = [
  { ext: "c", mode: "text/x-csrc" },
  { ext: "kt", mode: "text/x-kotlin" },
  { ext: "cpp", mode: "text/x-c++src" },
  { ext: "m", mode: "text/x-objectivec" },
  { ext: "rs", mode: "text/x-rustsrc" },
  { ext: "hx", mode: "text/x-haxe" },
];

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
    return { name: "text" };
  }

  const extension = source.displayURL.fileExtension;

  if (content.type !== "text") {
    return { name: "text" };
  }

  const { contentType, value: text } = content;

  if (extension === "jsx" || (symbols && symbols.hasJsx)) {
    if (symbols && symbols.hasTypes) {
      return { name: "text/typescript-jsx" };
    }
    return { name: "jsx" };
  }

  if (symbols && symbols.hasTypes) {
    if (symbols.hasJsx) {
      return { name: "text/typescript-jsx" };
    }

    return { name: "text/typescript" };
  }

  // check for C and other non JS languages
  const result = languageMimeMap.find(({ ext }) => extension === ext);
  if (result !== undefined) {
    return { name: result.mode };
  }

  // if the url ends with a known Javascript-like URL, provide JavaScript mode.
  // uses the first part of the URL to ignore query string
  if (javascriptLikeExtensions.find(ext => ext === extension)) {
    return { name: "javascript" };
  }

  // Use HTML mode for files in which the first non whitespace
  // character is `<` regardless of extension.
  const isHTMLLike = text.match(/^\s*</);
  if (!contentType) {
    if (isHTMLLike) {
      return { name: "htmlmixed" };
    }
    return { name: "text" };
  }

  // // @flow or /* @flow */
  if (text.match(/^\s*(\/\/ @flow|\/\* @flow \*\/)/)) {
    return contentTypeModeMap["text/typescript"];
  }

  if (/script|elm|jsx|clojure|wasm|html/.test(contentType)) {
    if (contentType in contentTypeModeMap) {
      return contentTypeModeMap[contentType];
    }

    return contentTypeModeMap["text/javascript"];
  }

  if (isHTMLLike) {
    return { name: "htmlmixed" };
  }

  return { name: "text" };
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
    return doc;
  }

  const content = sourceTextContent.value;
  let editorText;
  if (content.type === "wasm") {
    const wasmLines = renderWasmText(source.id, content);
    // cm will try to split into lines anyway, saving memory
    editorText = { split: () => wasmLines, match: () => false };
  } else {
    editorText = content.value;
  }

  const doc = editor.createDocument(
    editorText,
    getMode(source, sourceTextContent, symbols)
  );
  setDocument(source.id, doc);
  editor.replaceDocument(doc);

  updateLineNumberFormat(editor, source.id);
}
