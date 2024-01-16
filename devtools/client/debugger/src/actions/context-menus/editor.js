/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { showMenu } from "../../context-menu/menu";

import { copyToTheClipboard } from "../../utils/clipboard";
import {
  isPretty,
  getRawSourceURL,
  getFilename,
  shouldBlackbox,
  findBlackBoxRange,
} from "../../utils/source";
import { toSourceLine } from "../../utils/editor/index";
import { downloadFile } from "../../utils/utils";
import { features } from "../../utils/prefs";
import { isFulfilled } from "../../utils/async-value";

import { createBreakpointItems } from "./editor-breakpoint";

import {
  getPrettySource,
  getIsCurrentThreadPaused,
  isSourceWithMap,
  getBlackBoxRanges,
  isSourceOnSourceMapIgnoreList,
  isSourceMapIgnoreListEnabled,
  getEditorWrapping,
} from "../../selectors/index";

import { continueToHere } from "../../actions/pause/continueToHere";
import { jumpToMappedLocation } from "../../actions/sources/select";
import {
  showSource,
  toggleInlinePreview,
  toggleEditorWrapping,
} from "../../actions/ui";
import { toggleBlackBox } from "../../actions/sources/blackbox";
import { addExpression } from "../../actions/expressions";
import { evaluateInConsole } from "../../actions/toolbox";

export function showEditorContextMenu(event, editor, location) {
  return async ({ dispatch, getState }) => {
    const { source } = location;
    const state = getState();
    const blackboxedRanges = getBlackBoxRanges(state);
    const isPaused = getIsCurrentThreadPaused(state);
    const hasMappedLocation =
      (source.isOriginal ||
        isSourceWithMap(state, source.id) ||
        isPretty(source)) &&
      !getPrettySource(state, source.id);
    const isSourceOnIgnoreList =
      isSourceMapIgnoreListEnabled(state) &&
      isSourceOnSourceMapIgnoreList(state, source);
    const editorWrappingEnabled = getEditorWrapping(state);

    showMenu(
      event,
      editorMenuItems({
        blackboxedRanges,
        hasMappedLocation,
        location,
        isPaused,
        editorWrappingEnabled,
        selectionText: editor.codeMirror.getSelection().trim(),
        isTextSelected: editor.codeMirror.somethingSelected(),
        editor,
        isSourceOnIgnoreList,
        dispatch,
      })
    );
  };
}

export function showEditorGutterContextMenu(event, editor, location, lineText) {
  return async ({ dispatch, getState }) => {
    const { source } = location;
    const state = getState();
    const blackboxedRanges = getBlackBoxRanges(state);
    const isPaused = getIsCurrentThreadPaused(state);
    const isSourceOnIgnoreList =
      isSourceMapIgnoreListEnabled(state) &&
      isSourceOnSourceMapIgnoreList(state, source);

    showMenu(event, [
      ...createBreakpointItems(location, lineText, dispatch),
      { type: "separator" },
      continueToHereItem(location, isPaused, dispatch),
      { type: "separator" },
      blackBoxLineMenuItem(
        source,
        editor,
        blackboxedRanges,
        isSourceOnIgnoreList,
        location.line,
        dispatch
      ),
    ]);
  };
}

// Menu Items
const continueToHereItem = (location, isPaused, dispatch) => ({
  accesskey: L10N.getStr("editor.continueToHere.accesskey"),
  disabled: !isPaused,
  click: () => dispatch(continueToHere(location)),
  id: "node-menu-continue-to-here",
  label: L10N.getStr("editor.continueToHere.label"),
});

const copyToClipboardItem = selectionText => ({
  id: "node-menu-copy-to-clipboard",
  label: L10N.getStr("copyToClipboard.label"),
  accesskey: L10N.getStr("copyToClipboard.accesskey"),
  disabled: selectionText.length === 0,
  click: () => copyToTheClipboard(selectionText),
});

const copySourceItem = selectedContent => ({
  id: "node-menu-copy-source",
  label: L10N.getStr("copySource.label"),
  accesskey: L10N.getStr("copySource.accesskey"),
  disabled: false,
  click: () =>
    selectedContent.type === "text" &&
    copyToTheClipboard(selectedContent.value),
});

const copySourceUri2Item = selectedSource => ({
  id: "node-menu-copy-source-url",
  label: L10N.getStr("copySourceUri2"),
  accesskey: L10N.getStr("copySourceUri2.accesskey"),
  disabled: !selectedSource.url,
  click: () => copyToTheClipboard(getRawSourceURL(selectedSource.url)),
});

const jumpToMappedLocationItem = (location, hasMappedLocation, dispatch) => ({
  id: "node-menu-jump",
  label: L10N.getFormatStr(
    "editor.jumpToMappedLocation1",
    location.source.isOriginal
      ? L10N.getStr("generated")
      : L10N.getStr("original")
  ),
  accesskey: L10N.getStr("editor.jumpToMappedLocation1.accesskey"),
  disabled: !hasMappedLocation,
  click: () => dispatch(jumpToMappedLocation(location)),
});

const showSourceMenuItem = (selectedSource, dispatch) => ({
  id: "node-menu-show-source",
  label: L10N.getStr("sourceTabs.revealInTree"),
  accesskey: L10N.getStr("sourceTabs.revealInTree.accesskey"),
  disabled: !selectedSource.url,
  click: () => dispatch(showSource(selectedSource.id)),
});

const blackBoxMenuItem = (
  selectedSource,
  blackboxedRanges,
  isSourceOnIgnoreList,
  dispatch
) => {
  const isBlackBoxed = !!blackboxedRanges[selectedSource.url];
  return {
    id: "node-menu-blackbox",
    label: isBlackBoxed
      ? L10N.getStr("ignoreContextItem.unignore")
      : L10N.getStr("ignoreContextItem.ignore"),
    accesskey: isBlackBoxed
      ? L10N.getStr("ignoreContextItem.unignore.accesskey")
      : L10N.getStr("ignoreContextItem.ignore.accesskey"),
    disabled: isSourceOnIgnoreList || !shouldBlackbox(selectedSource),
    click: () => dispatch(toggleBlackBox(selectedSource)),
  };
};

const blackBoxLineMenuItem = (
  selectedSource,
  editor,
  blackboxedRanges,
  isSourceOnIgnoreList,
  // the clickedLine is passed when the context menu
  // is opened from the gutter, it is not available when the
  // the context menu is opened from the editor.
  clickedLine = null,
  dispatch
) => {
  const { codeMirror } = editor;
  const from = codeMirror.getCursor("from");
  const to = codeMirror.getCursor("to");

  const startLine = clickedLine ?? toSourceLine(selectedSource.id, from.line);
  const endLine = clickedLine ?? toSourceLine(selectedSource.id, to.line);

  const blackboxRange = findBlackBoxRange(selectedSource, blackboxedRanges, {
    start: startLine,
    end: endLine,
  });

  const selectedLineIsBlackBoxed = !!blackboxRange;

  const isSingleLine = selectedLineIsBlackBoxed
    ? blackboxRange.start.line == blackboxRange.end.line
    : startLine == endLine;

  const isSourceFullyBlackboxed =
    blackboxedRanges[selectedSource.url] &&
    !blackboxedRanges[selectedSource.url].length;

  // The ignore/unignore line context menu item should be disabled when
  // 1) The source is on the sourcemap ignore list
  // 2) The whole source is blackboxed or
  // 3) Multiple lines are blackboxed or
  // 4) Multiple lines are selected in the editor
  const shouldDisable =
    isSourceOnIgnoreList || isSourceFullyBlackboxed || !isSingleLine;

  return {
    id: "node-menu-blackbox-line",
    label: !selectedLineIsBlackBoxed
      ? L10N.getStr("ignoreContextItem.ignoreLine")
      : L10N.getStr("ignoreContextItem.unignoreLine"),
    accesskey: !selectedLineIsBlackBoxed
      ? L10N.getStr("ignoreContextItem.ignoreLine.accesskey")
      : L10N.getStr("ignoreContextItem.unignoreLine.accesskey"),
    disabled: shouldDisable,
    click: () => {
      const selectionRange = {
        start: {
          line: startLine,
          column: clickedLine == null ? from.ch : 0,
        },
        end: {
          line: endLine,
          column: clickedLine == null ? to.ch : 0,
        },
      };

      dispatch(
        toggleBlackBox(
          selectedSource,
          !selectedLineIsBlackBoxed,
          selectedLineIsBlackBoxed ? [blackboxRange] : [selectionRange]
        )
      );
    },
  };
};

const blackBoxLinesMenuItem = (
  selectedSource,
  editor,
  blackboxedRanges,
  isSourceOnIgnoreList,
  clickedLine = null,
  dispatch
) => {
  const { codeMirror } = editor;
  const from = codeMirror.getCursor("from");
  const to = codeMirror.getCursor("to");

  const startLine = toSourceLine(selectedSource.id, from.line);
  const endLine = toSourceLine(selectedSource.id, to.line);

  const blackboxRange = findBlackBoxRange(selectedSource, blackboxedRanges, {
    start: startLine,
    end: endLine,
  });

  const selectedLinesAreBlackBoxed = !!blackboxRange;

  return {
    id: "node-menu-blackbox-lines",
    label: !selectedLinesAreBlackBoxed
      ? L10N.getStr("ignoreContextItem.ignoreLines")
      : L10N.getStr("ignoreContextItem.unignoreLines"),
    accesskey: !selectedLinesAreBlackBoxed
      ? L10N.getStr("ignoreContextItem.ignoreLines.accesskey")
      : L10N.getStr("ignoreContextItem.unignoreLines.accesskey"),
    disabled: isSourceOnIgnoreList,
    click: () => {
      const selectionRange = {
        start: {
          line: startLine,
          column: from.ch,
        },
        end: {
          line: endLine,
          column: to.ch,
        },
      };

      dispatch(
        toggleBlackBox(
          selectedSource,
          !selectedLinesAreBlackBoxed,
          selectedLinesAreBlackBoxed ? [blackboxRange] : [selectionRange]
        )
      );
    },
  };
};

const watchExpressionItem = (selectedSource, selectionText, dispatch) => ({
  id: "node-menu-add-watch-expression",
  label: L10N.getStr("expressions.label"),
  accesskey: L10N.getStr("expressions.accesskey"),
  click: () => dispatch(addExpression(selectionText)),
});

const evaluateInConsoleItem = (selectedSource, selectionText, dispatch) => ({
  id: "node-menu-evaluate-in-console",
  label: L10N.getStr("evaluateInConsole.label"),
  click: () => dispatch(evaluateInConsole(selectionText)),
});

const downloadFileItem = (selectedSource, selectedContent) => ({
  id: "node-menu-download-file",
  label: L10N.getStr("downloadFile.label"),
  accesskey: L10N.getStr("downloadFile.accesskey"),
  click: () => downloadFile(selectedContent, getFilename(selectedSource)),
});

const inlinePreviewItem = dispatch => ({
  id: "node-menu-inline-preview",
  label: features.inlinePreview
    ? L10N.getStr("inlinePreview.hide.label")
    : L10N.getStr("inlinePreview.show.label"),
  click: () => dispatch(toggleInlinePreview(!features.inlinePreview)),
});

const editorWrappingItem = (editorWrappingEnabled, dispatch) => ({
  id: "node-menu-editor-wrapping",
  label: editorWrappingEnabled
    ? L10N.getStr("editorWrapping.hide.label")
    : L10N.getStr("editorWrapping.show.label"),
  click: () => dispatch(toggleEditorWrapping(!editorWrappingEnabled)),
});

function editorMenuItems({
  blackboxedRanges,
  location,
  selectionText,
  hasMappedLocation,
  isTextSelected,
  isPaused,
  editorWrappingEnabled,
  editor,
  isSourceOnIgnoreList,
  dispatch,
}) {
  const items = [];

  const { source } = location;

  const content =
    source.content && isFulfilled(source.content) ? source.content.value : null;

  items.push(
    jumpToMappedLocationItem(location, hasMappedLocation, dispatch),
    continueToHereItem(location, isPaused, dispatch),
    { type: "separator" },
    copyToClipboardItem(selectionText),
    ...(!source.isWasm
      ? [
          ...(content ? [copySourceItem(content)] : []),
          copySourceUri2Item(source),
        ]
      : []),
    ...(content ? [downloadFileItem(source, content)] : []),
    { type: "separator" },
    showSourceMenuItem(source, dispatch),
    { type: "separator" },
    blackBoxMenuItem(source, blackboxedRanges, isSourceOnIgnoreList, dispatch)
  );

  const startLine = toSourceLine(
    source.id,
    editor.codeMirror.getCursor("from").line
  );
  const endLine = toSourceLine(
    source.id,
    editor.codeMirror.getCursor("to").line
  );

  // Find any blackbox ranges that exist for the selected lines
  const blackboxRange = findBlackBoxRange(source, blackboxedRanges, {
    start: startLine,
    end: endLine,
  });

  const isMultiLineSelection = blackboxRange
    ? blackboxRange.start.line !== blackboxRange.end.line
    : startLine !== endLine;

  // When the range is defined and is an empty array,
  // the whole source is blackboxed
  const theWholeSourceIsBlackBoxed =
    blackboxedRanges[source.url] && !blackboxedRanges[source.url].length;

  if (!theWholeSourceIsBlackBoxed) {
    const blackBoxSourceLinesMenuItem = isMultiLineSelection
      ? blackBoxLinesMenuItem
      : blackBoxLineMenuItem;

    items.push(
      blackBoxSourceLinesMenuItem(
        source,
        editor,
        blackboxedRanges,
        isSourceOnIgnoreList,
        null,
        dispatch
      )
    );
  }

  if (isTextSelected) {
    items.push(
      { type: "separator" },
      watchExpressionItem(source, selectionText, dispatch),
      evaluateInConsoleItem(source, selectionText, dispatch)
    );
  }

  items.push(
    { type: "separator" },
    inlinePreviewItem(dispatch),
    editorWrappingItem(editorWrappingEnabled, dispatch)
  );

  return items;
}
