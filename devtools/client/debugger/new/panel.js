/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Task } = require("devtools/shared/task");
const { LocalizationHelper } = require("devtools/shared/l10n");
const { gDevTools } = require("devtools/client/framework/devtools");
const { gDevToolsBrowser } = require("devtools/client/framework/devtools-browser");
const { TargetFactory } = require("devtools/client/framework/target");
const { Toolbox } = require("devtools/client/framework/toolbox");
loader.lazyRequireGetter(this, "openContentLink", "devtools/client/shared/link", true);

const DBG_STRINGS_URI = "devtools/client/locales/debugger.properties";
const L10N = new LocalizationHelper(DBG_STRINGS_URI);

function DebuggerPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this.panelWin.L10N = L10N;
  this.toolbox = toolbox;
}

DebuggerPanel.prototype = {
  open: async function() {
    const {
      actions,
      store,
      selectors,
      client
    } = await this.panelWin.Debugger.bootstrap({
      threadClient: this.toolbox.threadClient,
      tabTarget: this.toolbox.target,
      debuggerClient: this.toolbox.target.client,
      sourceMaps: this.toolbox.sourceMapService,
      toolboxActions: {
        // Open a link in a new browser tab.
        openLink: this.openLink.bind(this),
        openWorkerToolbox: this.openWorkerToolbox.bind(this),
        openElementInInspector: async function(grip) {
          await this.toolbox.initInspector();
          const onSelectInspector = this.toolbox.selectTool("inspector");
          const onGripNodeToFront = this.toolbox.walker.gripToNodeFront(grip);
          const [
            front,
            inspector,
          ] = await Promise.all([onGripNodeToFront, onSelectInspector]);

          const onInspectorUpdated = inspector.once("inspector-updated");
          const onNodeFrontSet = this.toolbox.selection
            .setNodeFront(front, { reason: "debugger" });

          return Promise.all([onNodeFrontSet, onInspectorUpdated]);
        }.bind(this),
        onReload: this.onReload.bind(this)
      }
    });

    this._actions = actions;
    this._store = store;
    this._selectors = selectors;
    this._client = client;
    this.isReady = true;

    this.panelWin.document.addEventListener("drag:start", this.toolbox.toggleDragging);
    this.panelWin.document.addEventListener("drag:end", this.toolbox.toggleDragging);

    return this;
  },

  getVarsForTests() {
    return {
      store: this._store,
      selectors: this._selectors,
      actions: this._actions,
      client: this._client
    };
  },

  _getState: function() {
    return this._store.getState();
  },

  openLink: function(url) {
    openContentLink(url);
  },

  openWorkerToolbox: function(workerTargetFront) {
    return gDevToolsBrowser.openWorkerToolbox(workerTargetFront, "jsdebugger");
  },

  getFrames: function() {
    let frames = this._selectors.getFrames(this._getState());

    // Frames is null when the debugger is not paused.
    if (!frames) {
      return {
        frames: [],
        selected: -1
      };
    }

    const selectedFrame = this._selectors.getSelectedFrame(this._getState());
    const selected = frames.findIndex(frame => frame.id == selectedFrame.id);

    frames.forEach(frame => {
      frame.actor = frame.id;
    });

    return { frames, selected };
  },

  getMappedExpression(expression) {
    return this._actions.getMappedExpression(expression);
  },

  isPaused() {
    return this._selectors.isPaused(this._getState());
  },

  selectSource(url, line) {
    this._actions.selectSourceURL(url, { line });
  },

  getSource(sourceURL) {
    return this._selectors.getSourceByURL(this._getState(), sourceURL);
  },

  onReload: function() {
    this.emit("reloaded");
  },

  destroy: function() {
    this.panelWin.Debugger.destroy();
    this.emit("destroyed");
  }
};

exports.DebuggerPanel = DebuggerPanel;
