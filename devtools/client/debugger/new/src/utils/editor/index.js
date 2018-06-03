"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _sourceDocuments = require("./source-documents");

Object.keys(_sourceDocuments).forEach(function (key) {
  if (key === "default" || key === "__esModule") return;
  Object.defineProperty(exports, key, {
    enumerable: true,
    get: function () {
      return _sourceDocuments[key];
    }
  });
});

var _getTokenLocation = require("./get-token-location");

Object.keys(_getTokenLocation).forEach(function (key) {
  if (key === "default" || key === "__esModule") return;
  Object.defineProperty(exports, key, {
    enumerable: true,
    get: function () {
      return _getTokenLocation[key];
    }
  });
});

var _sourceSearch = require("./source-search");

Object.keys(_sourceSearch).forEach(function (key) {
  if (key === "default" || key === "__esModule") return;
  Object.defineProperty(exports, key, {
    enumerable: true,
    get: function () {
      return _sourceSearch[key];
    }
  });
});

var _ui = require("../ui");

Object.keys(_ui).forEach(function (key) {
  if (key === "default" || key === "__esModule") return;
  Object.defineProperty(exports, key, {
    enumerable: true,
    get: function () {
      return _ui[key];
    }
  });
});
exports.getEditor = getEditor;
exports.removeEditor = removeEditor;
exports.shouldShowPrettyPrint = shouldShowPrettyPrint;
exports.shouldShowFooter = shouldShowFooter;
exports.traverseResults = traverseResults;
exports.toEditorLine = toEditorLine;
exports.toEditorPosition = toEditorPosition;
exports.toEditorRange = toEditorRange;
exports.toSourceLine = toSourceLine;
exports.scrollToColumn = scrollToColumn;
exports.markText = markText;
exports.lineAtHeight = lineAtHeight;
exports.getSourceLocationFromMouseEvent = getSourceLocationFromMouseEvent;
exports.forEachLine = forEachLine;
exports.removeLineClass = removeLineClass;
exports.clearLineClass = clearLineClass;
exports.getTextForLine = getTextForLine;
exports.getCursorLine = getCursorLine;

var _createEditor = require("./create-editor");

var _source = require("../source");

var _wasm = require("../wasm");

var _devtoolsSourceMap = require("devtools/client/shared/source-map/index.js");

function _objectSpread(target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i] != null ? arguments[i] : {}; var ownKeys = Object.keys(source); if (typeof Object.getOwnPropertySymbols === 'function') { ownKeys = ownKeys.concat(Object.getOwnPropertySymbols(source).filter(function (sym) { return Object.getOwnPropertyDescriptor(source, sym).enumerable; })); } ownKeys.forEach(function (key) { _defineProperty(target, key, source[key]); }); } return target; }

function _defineProperty(obj, key, value) { if (key in obj) { Object.defineProperty(obj, key, { value: value, enumerable: true, configurable: true, writable: true }); } else { obj[key] = value; } return obj; }

let editor;

function getEditor() {
  if (editor) {
    return editor;
  }

  editor = (0, _createEditor.createEditor)();
  return editor;
}

function removeEditor() {
  editor = null;
}

function shouldShowPrettyPrint(selectedSource) {
  if (!selectedSource) {
    return false;
  }

  return (0, _source.shouldPrettyPrint)(selectedSource);
}

function shouldShowFooter(selectedSource, horizontal) {
  if (!horizontal) {
    return true;
  }

  if (!selectedSource) {
    return false;
  }

  return shouldShowPrettyPrint(selectedSource) || (0, _devtoolsSourceMap.isOriginalId)(selectedSource.get("id"));
}

function traverseResults(e, ctx, query, dir, modifiers) {
  e.stopPropagation();
  e.preventDefault();

  if (dir == "prev") {
    (0, _sourceSearch.findPrev)(ctx, query, true, modifiers);
  } else if (dir == "next") {
    (0, _sourceSearch.findNext)(ctx, query, true, modifiers);
  }
}

function toEditorLine(sourceId, lineOrOffset) {
  if ((0, _wasm.isWasm)(sourceId)) {
    // TODO ensure offset is always "mappable" to edit line.
    return (0, _wasm.wasmOffsetToLine)(sourceId, lineOrOffset) || 0;
  }

  return lineOrOffset ? lineOrOffset - 1 : 1;
}

function toEditorPosition(location) {
  return {
    line: toEditorLine(location.sourceId, location.line),
    column: (0, _wasm.isWasm)(location.sourceId) || !location.column ? 0 : location.column
  };
}

function toEditorRange(sourceId, location) {
  const {
    start,
    end
  } = location;
  return {
    start: toEditorPosition(_objectSpread({}, start, {
      sourceId
    })),
    end: toEditorPosition(_objectSpread({}, end, {
      sourceId
    }))
  };
}

function toSourceLine(sourceId, line) {
  return (0, _wasm.isWasm)(sourceId) ? (0, _wasm.lineToWasmOffset)(sourceId, line) : line + 1;
}

function scrollToColumn(codeMirror, line, column) {
  const {
    top,
    left
  } = codeMirror.charCoords({
    line: line,
    ch: column
  }, "local");

  if (!isVisible(codeMirror, top, left)) {
    const scroller = codeMirror.getScrollerElement();
    const centeredX = Math.max(left - scroller.offsetWidth / 2, 0);
    const centeredY = Math.max(top - scroller.offsetHeight / 2, 0);
    codeMirror.scrollTo(centeredX, centeredY);
  }
}

function isVisible(codeMirror, top, left) {
  function withinBounds(x, min, max) {
    return x >= min && x <= max;
  }

  const scrollArea = codeMirror.getScrollInfo();
  const charWidth = codeMirror.defaultCharWidth();
  const inXView = withinBounds(left, scrollArea.left, scrollArea.left + (scrollArea.clientWidth - 30) - charWidth);
  const fontHeight = codeMirror.defaultTextHeight();
  const inYView = withinBounds(top, scrollArea.top, scrollArea.top + scrollArea.clientHeight - fontHeight);
  return inXView && inYView;
}

function markText(_editor, className, {
  start,
  end
}) {
  return _editor.codeMirror.markText({
    ch: start.column,
    line: start.line
  }, {
    ch: end.column,
    line: end.line
  }, {
    className
  });
}

function lineAtHeight(_editor, sourceId, event) {
  const _editorLine = _editor.codeMirror.lineAtHeight(event.clientY);

  return toSourceLine(sourceId, _editorLine);
}

function getSourceLocationFromMouseEvent(_editor, selectedLocation, e) {
  const {
    line,
    ch
  } = _editor.codeMirror.coordsChar({
    left: e.clientX,
    top: e.clientY
  });

  return {
    sourceId: selectedLocation.sourceId,
    line: line + 1,
    column: ch + 1
  };
}

function forEachLine(codeMirror, iter) {
  codeMirror.operation(() => {
    codeMirror.doc.iter(0, codeMirror.lineCount(), iter);
  });
}

function removeLineClass(codeMirror, line, className) {
  codeMirror.removeLineClass(line, "line", className);
}

function clearLineClass(codeMirror, className) {
  forEachLine(codeMirror, line => {
    removeLineClass(codeMirror, line, className);
  });
}

function getTextForLine(codeMirror, line) {
  return codeMirror.getLine(line - 1).trim();
}

function getCursorLine(codeMirror) {
  return codeMirror.getCursor().line;
}