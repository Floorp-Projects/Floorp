/* vim:set ts=2 sw=2 sts=2 et tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = ["SourceEditorUI"];

/**
 * The Source Editor component user interface.
 */
this.SourceEditorUI = function SourceEditorUI(aEditor)
{
  this.editor = aEditor;
  this._onDirtyChanged = this._onDirtyChanged.bind(this);
}

SourceEditorUI.prototype = {
  /**
   * Initialize the user interface. This is called by the SourceEditor.init()
   * method.
   */
  init: function SEU_init()
  {
    this._ownerWindow = this.editor.parentElement.ownerDocument.defaultView;
  },

  /**
   * The UI onReady function is executed once the Source Editor completes
   * initialization and it is ready for usage. Currently this code sets up the
   * nsIController.
   */
  onReady: function SEU_onReady()
  {
    if (this._ownerWindow.controllers) {
      this._controller = new SourceEditorController(this.editor);
      this._ownerWindow.controllers.insertControllerAt(0, this._controller);
      this.editor.addEventListener(this.editor.EVENTS.DIRTY_CHANGED,
                                   this._onDirtyChanged);
    }
  },

  /**
   * The "go to line" command UI. This displays a prompt that allows the user to
   * input the line number to jump to.
   */
  gotoLine: function SEU_gotoLine()
  {
    let oldLine = this.editor.getCaretPosition ?
                  this.editor.getCaretPosition().line : null;
    let newLine = {value: oldLine !== null ? oldLine + 1 : ""};

    let result = Services.prompt.prompt(this._ownerWindow,
      SourceEditorUI.strings.GetStringFromName("gotoLineCmd.promptTitle"),
      SourceEditorUI.strings.GetStringFromName("gotoLineCmd.promptMessage"),
      newLine, null, {});

    newLine.value = parseInt(newLine.value);
    if (result && !isNaN(newLine.value) && --newLine.value != oldLine) {
      if (this.editor.getLineCount) {
        let lines = this.editor.getLineCount() - 1;
        this.editor.setCaretPosition(Math.max(0, Math.min(lines, newLine.value)));
      } else {
        this.editor.setCaretPosition(Math.max(0, newLine.value));
      }
    }

    return true;
  },

  /**
   * The "find" command UI. This displays a prompt that allows the user to input
   * the string to search for in the code. By default the current selection is
   * used as a search string, or the last search string.
   */
  find: function SEU_find()
  {
    let str = {value: this.editor.getSelectedText()};
    if (!str.value && this.editor.lastFind) {
      str.value = this.editor.lastFind.str;
    }

    let result = Services.prompt.prompt(this._ownerWindow,
      SourceEditorUI.strings.GetStringFromName("findCmd.promptTitle"),
      SourceEditorUI.strings.GetStringFromName("findCmd.promptMessage"),
      str, null, {});

    if (result && str.value) {
      let start = this.editor.getSelection().end;
      let pos = this.editor.find(str.value, {ignoreCase: true, start: start});
      if (pos == -1) {
        this.editor.find(str.value, {ignoreCase: true});
      }
      this._onFind();
    }

    return true;
  },

  /**
   * Find the next occurrence of the last search string.
   */
  findNext: function SEU_findNext()
  {
    let lastFind = this.editor.lastFind;
    if (lastFind) {
      this.editor.findNext(true);
      this._onFind();
    }

    return true;
  },

  /**
   * Find the previous occurrence of the last search string.
   */
  findPrevious: function SEU_findPrevious()
  {
    let lastFind = this.editor.lastFind;
    if (lastFind) {
      this.editor.findPrevious(true);
      this._onFind();
    }

    return true;
  },

  /**
   * This executed after each find/findNext/findPrevious operation.
   * @private
   */
  _onFind: function SEU__onFind()
  {
    let lastFind = this.editor.lastFind;
    if (lastFind && lastFind.index > -1) {
      this.editor.setSelection(lastFind.index, lastFind.index + lastFind.str.length);
    }

    if (this._ownerWindow.goUpdateCommand) {
      this._ownerWindow.goUpdateCommand("cmd_findAgain");
      this._ownerWindow.goUpdateCommand("cmd_findPrevious");
    }
  },

  /**
   * This is executed after each undo/redo operation.
   * @private
   */
  _onUndoRedo: function SEU__onUndoRedo()
  {
    if (this._ownerWindow.goUpdateCommand) {
      this._ownerWindow.goUpdateCommand("se-cmd-undo");
      this._ownerWindow.goUpdateCommand("se-cmd-redo");
    }
  },

  /**
   * The DirtyChanged event handler for the editor. This tracks the editor state
   * changes to make sure the Source Editor overlay Undo/Redo commands are kept
   * up to date.
   * @private
   */
  _onDirtyChanged: function SEU__onDirtyChanged()
  {
    this._onUndoRedo();
  },

  /**
   * Destroy the SourceEditorUI instance. This is called by the
   * SourceEditor.destroy() method.
   */
  destroy: function SEU_destroy()
  {
    if (this._ownerWindow.controllers) {
      this.editor.removeEventListener(this.editor.EVENTS.DIRTY_CHANGED,
                                      this._onDirtyChanged);
    }

    this._ownerWindow = null;
    this.editor = null;
    this._controller = null;
  },
};

/**
 * The Source Editor nsIController implements features that need to be available
 * from XUL commands.
 *
 * @constructor
 * @param object aEditor
 *        SourceEditor object instance for which the controller is instanced.
 */
function SourceEditorController(aEditor)
{
  this._editor = aEditor;
}

SourceEditorController.prototype = {
  /**
   * Check if a command is supported by the controller.
   *
   * @param string aCommand
   *        The command name you want to check support for.
   * @return boolean
   *         True if the command is supported, false otherwise.
   */
  supportsCommand: function SEC_supportsCommand(aCommand)
  {
    let result;

    switch (aCommand) {
      case "cmd_find":
      case "cmd_findAgain":
      case "cmd_findPrevious":
      case "cmd_gotoLine":
      case "se-cmd-undo":
      case "se-cmd-redo":
      case "se-cmd-cut":
      case "se-cmd-paste":
      case "se-cmd-delete":
      case "se-cmd-selectAll":
        result = true;
        break;
      default:
        result = false;
        break;
    }

    return result;
  },

  /**
   * Check if a command is enabled or not.
   *
   * @param string aCommand
   *        The command name you want to check if it is enabled or not.
   * @return boolean
   *         True if the command is enabled, false otherwise.
   */
  isCommandEnabled: function SEC_isCommandEnabled(aCommand)
  {
    let result;

    switch (aCommand) {
      case "cmd_find":
      case "cmd_gotoLine":
      case "se-cmd-selectAll":
        result = true;
        break;
      case "cmd_findAgain":
      case "cmd_findPrevious":
        result = this._editor.lastFind && this._editor.lastFind.lastFound != -1;
        break;
      case "se-cmd-undo":
        result = this._editor.canUndo();
        break;
      case "se-cmd-redo":
        result = this._editor.canRedo();
        break;
      case "se-cmd-cut":
      case "se-cmd-delete": {
        let selection = this._editor.getSelection();
        result = selection.start != selection.end && !this._editor.readOnly;
        break;
      }
      case "se-cmd-paste": {
        let window = this._editor._view._frameWindow;
        let controller = window.controllers.getControllerForCommand("cmd_paste");
        result = !this._editor.readOnly &&
                 controller.isCommandEnabled("cmd_paste");
        break;
      }
      default:
        result = false;
        break;
    }

    return result;
  },

  /**
   * Perform a command.
   *
   * @param string aCommand
   *        The command name you want to execute.
   * @return void
   */
  doCommand: function SEC_doCommand(aCommand)
  {
    switch (aCommand) {
      case "cmd_find":
        this._editor.ui.find();
        break;
      case "cmd_findAgain":
        this._editor.ui.findNext();
        break;
      case "cmd_findPrevious":
        this._editor.ui.findPrevious();
        break;
      case "cmd_gotoLine":
        this._editor.ui.gotoLine();
        break;
      case "se-cmd-selectAll":
        this._editor._view.invokeAction("selectAll");
        break;
      case "se-cmd-undo":
        this._editor.undo();
        break;
      case "se-cmd-redo":
        this._editor.redo();
        break;
      case "se-cmd-cut":
        this._editor.ui._ownerWindow.goDoCommand("cmd_cut");
        break;
      case "se-cmd-paste":
        this._editor.ui._ownerWindow.goDoCommand("cmd_paste");
        break;
      case "se-cmd-delete": {
        let selection = this._editor.getSelection();
        this._editor.setText("", selection.start, selection.end);
        break;
      }
    }
  },

  onEvent: function() { }
};
