/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * The source editor exposes XUL commands that can be used when embedded in a XUL
 * document. This controller drives the availability and behavior of the commands. When
 * the editor input field is focused, this controller will update the matching menu item
 * entries found in application menus or context menus.
 */

/**
 * Returns a controller object that can be used for editor-specific commands:
 * - find
 * - find again
 * - go to line
 * - undo
 * - redo
 * - delete
 * - select all
 */
function createController(ed) {
  return {
    supportsCommand: function (cmd) {
      switch (cmd) {
        case "cmd_find":
        case "cmd_findAgain":
        case "cmd_gotoLine":
        case "cmd_undo":
        case "cmd_redo":
        case "cmd_delete":
        case "cmd_selectAll":
          return true;
      }

      return false;
    },

    isCommandEnabled: function (cmd) {
      let cm = ed.codeMirror;

      switch (cmd) {
        case "cmd_find":
        case "cmd_gotoLine":
        case "cmd_selectAll":
          return true;
        case "cmd_findAgain":
          return cm.state.search != null && cm.state.search.query != null;
        case "cmd_undo":
          return ed.canUndo();
        case "cmd_redo":
          return ed.canRedo();
        case "cmd_delete":
          return ed.somethingSelected();
      }

      return false;
    },

    doCommand: function (cmd) {
      let cm = ed.codeMirror;

      let map = {
        "cmd_selectAll": "selectAll",
        "cmd_find": "find",
        "cmd_undo": "undo",
        "cmd_redo": "redo",
        "cmd_delete": "delCharAfter",
        "cmd_findAgain": "findNext"
      };

      if (map[cmd]) {
        cm.execCommand(map[cmd]);
        return;
      }

      if (cmd == "cmd_gotoLine") {
        ed.jumpToLine();
      }
    },

    onEvent: function () {}
  };
}

/**
 * Create and insert a commands controller for the provided SourceEditor instance.
 */
function insertCommandsController(sourceEditor) {
  let input = sourceEditor.codeMirror.getInputField();
  let controller = createController(sourceEditor);
  input.controllers.insertControllerAt(0, controller);
}

module.exports = { insertCommandsController };
