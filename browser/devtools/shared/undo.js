/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A simple undo stack manager.
 *
 * Actions are added along with the necessary code to
 * reverse the action.
 *
 * @param function aChange Called whenever the size or position
 *   of the undo stack changes, to use for updating undo-related
 *   UI.
 * @param integer aMaxUndo Maximum number of undo steps.
 *   defaults to 50.
 */
function UndoStack(aMaxUndo)
{
  this.maxUndo = aMaxUndo || 50;
  this._stack = [];
}

exports.UndoStack = UndoStack;

UndoStack.prototype = {
  // Current index into the undo stack.  Is positioned after the last
  // currently-applied change.
  _index: 0,

  // The current batch depth (see startBatch() for details)
  _batchDepth: 0,

  destroy: function Undo_destroy()
  {
    this.uninstallController();
    delete this._stack;
  },

  /**
   * Start a collection of related changes.  Changes will be batched
   * together into one undo/redo item until endBatch() is called.
   *
   * Batches can be nested, in which case the outer batch will contain
   * all items from the inner batches.  This allows larger user
   * actions made up of a collection of smaller actions to be
   * undone as a single action.
   */
  startBatch: function Undo_startBatch()
  {
    if (this._batchDepth++ === 0) {
      this._batch = [];
    }
  },

  /**
   * End a batch of related changes, performing its action and adding
   * it to the undo stack.
   */
  endBatch: function Undo_endBatch()
  {
    if (--this._batchDepth > 0) {
      return;
    }

    // Cut off the end of the undo stack at the current index,
    // and the beginning to prevent a stack larger than maxUndo.
    let start = Math.max((this._index + 1) - this.maxUndo, 0);
    this._stack = this._stack.slice(start, this._index);

    let batch = this._batch;
    delete this._batch;
    let entry = {
      do: function() {
        for (let item of batch) {
          item.do();
        }
      },
      undo: function() {
        for (let i = batch.length - 1; i >= 0; i--) {
          batch[i].undo();
        }
      }
    };
    this._stack.push(entry);
    this._index = this._stack.length;
    entry.do();
    this._change();
  },

  /**
   * Perform an action, adding it to the undo stack.
   *
   * @param function aDo Called to perform the action.
   * @param function aUndo Called to reverse the action.
   */
  do: function Undo_do(aDo, aUndo) {
    this.startBatch();
    this._batch.push({ do: aDo, undo: aUndo });
    this.endBatch();
  },

  /*
   * Returns true if undo() will do anything.
   */
  canUndo: function Undo_canUndo()
  {
    return this._index > 0;
  },

  /**
   * Undo the top of the undo stack.
   *
   * @return true if an action was undone.
   */
  undo: function Undo_canUndo()
  {
    if (!this.canUndo()) {
      return false;
    }
    this._stack[--this._index].undo();
    this._change();
    return true;
  },

  /**
   * Returns true if redo() will do anything.
   */
  canRedo: function Undo_canRedo()
  {
    return this._stack.length > this._index;
  },

  /**
   * Redo the most recently undone action.
   *
   * @return true if an action was redone.
   */
  redo: function Undo_canRedo()
  {
    if (!this.canRedo()) {
      return false;
    }
    this._stack[this._index++].do();
    this._change();
    return true;
  },

  _change: function Undo__change()
  {
    if (this._controllerWindow) {
      this._controllerWindow.goUpdateCommand("cmd_undo");
      this._controllerWindow.goUpdateCommand("cmd_redo");
    }
  },

  /**
   * ViewController implementation for undo/redo.
   */

  /**
   * Install this object as a command controller.
   */
  installController: function Undo_installController(aControllerWindow)
  {
    this._controllerWindow = aControllerWindow;
    aControllerWindow.controllers.appendController(this);
  },

  /**
   * Uninstall this object from the command controller.
   */
  uninstallController: function Undo_uninstallController()
  {
    if (!this._controllerWindow) {
      return;
    }
    this._controllerWindow.controllers.removeController(this);
  },

  supportsCommand: function Undo_supportsCommand(aCommand)
  {
    return (aCommand == "cmd_undo" ||
            aCommand == "cmd_redo");
  },

  isCommandEnabled: function Undo_isCommandEnabled(aCommand)
  {
    switch(aCommand) {
      case "cmd_undo": return this.canUndo();
      case "cmd_redo": return this.canRedo();
    };
    return false;
  },

  doCommand: function Undo_doCommand(aCommand)
  {
    switch(aCommand) {
      case "cmd_undo": return this.undo();
      case "cmd_redo": return this.redo();
    }
  },

  onEvent: function Undo_onEvent(aEvent) {},
}
