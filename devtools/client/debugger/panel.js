/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu, Cr } = require("chrome");
const promise = require("promise");
const EventEmitter = require("devtools/shared/event-emitter");
const DevToolsUtils = require("devtools/shared/DevToolsUtils");

function DebuggerPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this._toolbox = toolbox;
  this._destroyer = null;

  this._view = this.panelWin.DebuggerView;
  this._controller = this.panelWin.DebuggerController;
  this._view._hostType = this._toolbox.hostType;
  this._controller._target = this.target;
  this._controller._toolbox = this._toolbox;

  this.handleHostChanged = this.handleHostChanged.bind(this);
  EventEmitter.decorate(this);
}

exports.DebuggerPanel = DebuggerPanel;

DebuggerPanel.prototype = {
  /**
   * Open is effectively an asynchronous constructor.
   *
   * @return object
   *         A promise that is resolved when the Debugger completes opening.
   */
  open: function () {
    let targetPromise;

    // Local debugging needs to make the target remote.
    if (!this.target.isRemote) {
      targetPromise = this.target.makeRemote();
      // Listen for tab switching events to manage focus when the content window
      // is paused and events suppressed.
      this.target.tab.addEventListener("TabSelect", this);
    } else {
      targetPromise = promise.resolve(this.target);
    }

    return targetPromise
      .then(() => this._controller.startupDebugger())
      .then(() => this._controller.connect())
      .then(() => {
        this._toolbox.on("host-changed", this.handleHostChanged);
        // Add keys from this document's keyset to the toolbox, so they
        // can work when the split console is focused.
        let keysToClone = ["resumeKey", "stepOverKey", "stepInKey", "stepOutKey"];
        for (let key of keysToClone) {
          let elm = this.panelWin.document.getElementById(key);
          let keycode = elm.getAttribute("keycode");
          let modifiers = elm.getAttribute("modifiers");
          let command = elm.getAttribute("command");
          let handler = this._view.Toolbar.getCommandHandler(command);

          let keyShortcut = this.translateToKeyShortcut(keycode, modifiers);
          this._toolbox.useKeyWithSplitConsole(keyShortcut, handler, "jsdebugger");
        }
        this.isReady = true;
        this.emit("ready");
        return this;
      })
      .catch(function onError(aReason) {
        DevToolsUtils.reportException("DebuggerPanel.prototype.open", aReason);
      });
  },

  /**
   * Translate a VK_ keycode, with modifiers, to a key shortcut that can be used with
   * shared/key-shortcut.
   *
   * @param {String} keycode
   *        The VK_* keycode to translate
   * @param {String} modifiers
   *        The list (blank-space separated) of modifiers applying to this keycode.
   * @return {String} a key shortcut ready to be used with shared/key-shortcut.js
   */
  translateToKeyShortcut: function (keycode, modifiers) {
    // Remove the VK_ prefix.
    keycode = keycode.replace("VK_", "");

    // Translate modifiers
    if (modifiers.includes("shift")) {
      keycode = "Shift+" + keycode;
    }
    if (modifiers.includes("alt")) {
      keycode = "Alt+" + keycode;
    }
    if (modifiers.includes("control")) {
      keycode = "Ctrl+" + keycode;
    }
    if (modifiers.includes("meta")) {
      keycode = "Cmd+" + keycode;
    }
    if (modifiers.includes("accel")) {
      keycode = "CmdOrCtrl+" + keycode;
    }

    return keycode;
  },

  // DevToolPanel API

  get target() {
    return this._toolbox.target;
  },

  destroy: function () {
    // Make sure this panel is not already destroyed.
    if (this._destroyer) {
      return this._destroyer;
    }

    if (!this.target.isRemote) {
      this.target.tab.removeEventListener("TabSelect", this);
    }

    return this._destroyer = this._controller.shutdownDebugger().then(() => {
      this.emit("destroyed");
    });
  },

  // DebuggerPanel API

  getMappedExpression(expression) {
    // No-op implementation since this feature doesn't exist in the older
    // debugger implementation.
    return expression;
  },

  isPaused() {
    let framesController = this.panelWin.DebuggerController.StackFrames;
    let thread = framesController.activeThread;
    return thread && thread.paused;
  },

  getFrames() {
    let framesController = this.panelWin.DebuggerController.StackFrames;
    let thread = framesController.activeThread;
    if (this.isPaused()) {
      return {
        frames: thread.cachedFrames,
        selected: framesController.currentFrameDepth,
      };
    }

    return null;
  },

  addBreakpoint: function (location) {
    const { actions } = this.panelWin;
    const { dispatch } = this._controller;

    return dispatch(actions.addBreakpoint(location));
  },

  removeBreakpoint: function (location) {
    const { actions } = this.panelWin;
    const { dispatch } = this._controller;

    return dispatch(actions.removeBreakpoint(location));
  },

  blackbox: function (source, flag) {
    const { actions } = this.panelWin;
    const { dispatch } = this._controller;
    return dispatch(actions.blackbox(source, flag));
  },

  handleHostChanged: function () {
    this._view.handleHostChanged(this._toolbox.hostType);
  },

  // nsIDOMEventListener API

  handleEvent: function (aEvent) {
    if (aEvent.target == this.target.tab &&
        this._controller.activeThread.state == "paused") {
      // Wait a tick for the content focus event to be delivered.
      DevToolsUtils.executeSoon(() => this._toolbox.focusTool("jsdebugger"));
    }
  }
};
