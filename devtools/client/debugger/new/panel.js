/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

function DebuggerPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this.toolbox = toolbox;
}

DebuggerPanel.prototype = {
  open: function() {
    let targetPromise;
    if (!this.toolbox.target.isRemote) {
      targetPromise = this.toolbox.target.makeRemote();
    } else {
      targetPromise = promise.resolve(this.toolbox.target);
    }

    return targetPromise.then(() => {
      this.panelWin.Debugger.bootstrap({
        threadClient: this.toolbox.threadClient,
        tabTarget: this.toolbox.target
      });
      return this;
    });
  },

  function _store() {
    return this.panelWin.Debugger.store;
  },

  function _getState() {
    return this._store().getState();
  },

  function _actions() {
    return this.panelWin.Debugger.actions;
  },

  function _selectors() {
    return this.panelWin.Debugger.selectors;
  },

  getFrames: function() {
    let frames = this._selectors().getFrames(this._getState()).toJS();
    const selectedFrame = this._selectors().getSelectedFrame(this._getState());
    const selected = frames.findIndex(frame => frame.id == selectedFrame.id);

    frames.forEach(frame => {
      frame.actor = frame.id;
    });

    return { frames, selected };
  },

  destroy: function() {
  }
};

exports.DebuggerPanel = DebuggerPanel;
