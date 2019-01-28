/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import { bindActionCreators } from "redux";
import { isOriginalId } from "devtools-source-map";

import { copyToTheClipboard } from "../../../utils/clipboard";
import {
  isPretty,
  getRawSourceURL,
  getFilename,
  shouldBlackbox
} from "../../../utils/source";

import { downloadFile } from "../../../utils/utils";

import actions from "../../../actions";

import type { Source, SourceLocation } from "../../../types";

function isMapped(selectedSource) {
  return isOriginalId(selectedSource.id) || !!selectedSource.sourceMapURL;
}

export const continueToHereItem = (
  location: SourceLocation,
  isPaused: boolean,
  editorActions: EditorItemActions
) => ({
  accesskey: L10N.getStr("editor.continueToHere.accesskey"),
  disabled: !isPaused,
  click: () => editorActions.continueToHere(location.line, location.column),
  id: "node-menu-continue-to-here",
  label: L10N.getStr("editor.continueToHere.label")
});

// menu items

const copyToClipboardItem = (
  selectedSource: Source,
  editorActions: EditorItemActions
) => {
  return {
    id: "node-menu-copy-to-clipboard",
    label: L10N.getStr("copyToClipboard.label"),
    accesskey: L10N.getStr("copyToClipboard.accesskey"),
    disabled: false,
    click: () =>
      !selectedSource.isWasm &&
      typeof selectedSource.text == "string" &&
      copyToTheClipboard(selectedSource.text)
  };
};

const copySourceItem = (
  selectedSource: Source,
  selectionText: string,
  editorActions: EditorItemActions
) => {
  if (selectedSource.isWasm) {
    return;
  }

  return {
    id: "node-menu-copy-source",
    label: L10N.getStr("copySource.label"),
    accesskey: L10N.getStr("copySource.accesskey"),
    disabled: selectionText.length === 0,
    click: () => copyToTheClipboard(selectionText)
  };
};

const copySourceUri2Item = (
  selectedSource: Source,
  editorActions: EditorItemActions
) => ({
  id: "node-menu-copy-source-url",
  label: L10N.getStr("copySourceUri2"),
  accesskey: L10N.getStr("copySourceUri2.accesskey"),
  disabled: !selectedSource.url,
  click: () => copyToTheClipboard(getRawSourceURL(selectedSource.url))
});

const jumpToMappedLocationItem = (
  selectedSource: Source,
  location: SourceLocation,
  hasPrettySource: boolean,
  editorActions: EditorItemActions
) => ({
  id: "node-menu-jump",
  label: L10N.getFormatStr(
    "editor.jumpToMappedLocation1",
    isOriginalId(selectedSource.id)
      ? L10N.getStr("generated")
      : L10N.getStr("original")
  ),
  accesskey: L10N.getStr("editor.jumpToMappedLocation1.accesskey"),
  disabled:
    (!isMapped(selectedSource) && !isPretty(selectedSource)) || hasPrettySource,
  click: () => editorActions.jumpToMappedLocation(location)
});

const showSourceMenuItem = (
  selectedSource: Source,
  editorActions: EditorItemActions
) => ({
  id: "node-menu-show-source",
  label: L10N.getStr("sourceTabs.revealInTree"),
  accesskey: L10N.getStr("sourceTabs.revealInTree.accesskey"),
  disabled: !selectedSource.url,
  click: () => editorActions.showSource(selectedSource.id)
});

const blackBoxMenuItem = (
  selectedSource: Source,
  editorActions: EditorItemActions
) => ({
  id: "node-menu-blackbox",
  label: selectedSource.isBlackBoxed
    ? L10N.getStr("sourceFooter.unblackbox")
    : L10N.getStr("sourceFooter.blackbox"),
  accesskey: L10N.getStr("sourceFooter.blackbox.accesskey"),
  disabled: !shouldBlackbox(selectedSource),
  click: () => editorActions.toggleBlackBox(selectedSource)
});

const watchExpressionItem = (
  selectedSource: Source,
  selectionText: string,
  editorActions: EditorItemActions
) => ({
  id: "node-menu-add-watch-expression",
  label: L10N.getStr("expressions.label"),
  accesskey: L10N.getStr("expressions.accesskey"),
  click: () => editorActions.addExpression(selectionText)
});

const evaluateInConsoleItem = (
  selectedSource: Source,
  selectionText: string,
  editorActions: EditorItemActions
) => ({
  id: "node-menu-evaluate-in-console",
  label: L10N.getStr("evaluateInConsole.label"),
  click: () => editorActions.evaluateInConsole(selectionText)
});

const downloadFileItem = (
  selectedSource: Source,
  editorActions: EditorItemActions
) => {
  return {
    id: "node-menu-download-file",
    label: L10N.getStr("downloadFile.label"),
    accesskey: L10N.getStr("downloadFile.accesskey"),
    click: () => downloadFile(selectedSource, getFilename(selectedSource))
  };
};

export function editorMenuItems({
  editorActions,
  selectedSource,
  location,
  selectionText,
  hasPrettySource,
  isTextSelected,
  isPaused
}: {
  editorActions: EditorItemActions,
  selectedSource: Source,
  location: SourceLocation,
  selectionText: string,
  hasPrettySource: boolean,
  isTextSelected: boolean,
  isPaused: boolean
}) {
  const items = [];

  items.push(
    jumpToMappedLocationItem(
      selectedSource,
      location,
      hasPrettySource,
      editorActions
    ),
    continueToHereItem(location, isPaused, editorActions),
    { type: "separator" },
    copyToClipboardItem(selectedSource, editorActions),
    copySourceItem(selectedSource, selectionText, editorActions),
    copySourceUri2Item(selectedSource, editorActions),
    downloadFileItem(selectedSource, editorActions),
    { type: "separator" },
    showSourceMenuItem(selectedSource, editorActions),
    blackBoxMenuItem(selectedSource, editorActions)
  );

  if (isTextSelected) {
    items.push(
      { type: "separator" },
      watchExpressionItem(selectedSource, selectionText, editorActions),
      evaluateInConsoleItem(selectedSource, selectionText, editorActions)
    );
  }

  return items;
}

export type EditorItemActions = {
  addExpression: typeof actions.addExpression,
  continueToHere: typeof actions.continueToHere,
  evaluateInConsole: typeof actions.evaluateInConsole,
  flashLineRange: typeof actions.flashLineRange,
  jumpToMappedLocation: typeof actions.jumpToMappedLocation,
  showSource: typeof actions.showSource,
  toggleBlackBox: typeof actions.toggleBlackBox
};

export function editorItemActions(dispatch: Function) {
  return bindActionCreators(
    {
      addExpression: actions.addExpression,
      continueToHere: actions.continueToHere,
      evaluateInConsole: actions.evaluateInConsole,
      flashLineRange: actions.flashLineRange,
      jumpToMappedLocation: actions.jumpToMappedLocation,
      showSource: actions.showSource,
      toggleBlackBox: actions.toggleBlackBox
    },
    dispatch
  );
}
