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
      targetPromise = Promise.resolve(this.toolbox.target);
    }

    return targetPromise.then(() => {
      this.panelWin.Debugger.bootstrap({
        threadClient: this.toolbox.threadClient,
        tabTarget: this.toolbox.target
      });
      return this;
    });
  },

  _store: function() {
    return this.panelWin.Debugger.store;
  },

  _getState: function() {
    return this._store().getState();
  },

  _actions: function() {
    return this.panelWin.Debugger.actions;
  },

  _selectors: function() {
    return this.panelWin.Debugger.selectors;
  },

  getFrames: function() {
    let frames = this._selectors().getFrames(this._getState());

    // frames is an empty array when the debugger is not paused
    if (!frames.toJS) {
      return {
        frames: [],
        selected: -1
      }
    }

    frames = frames.toJS();
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
