/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { LocalizationHelper } = require("devtools/shared/l10n");
loader.lazyRequireGetter(
  this,
  "openContentLink",
  "devtools/client/shared/link",
  true
);
loader.lazyRequireGetter(
  this,
  "features",
  "devtools/client/debugger/src/utils/prefs",
  true
);

const DBG_STRINGS_URI = "devtools/client/locales/debugger.properties";
const L10N = new LocalizationHelper(DBG_STRINGS_URI);

function DebuggerPanel(iframeWindow, toolbox) {
  this.panelWin = iframeWindow;
  this.panelWin.L10N = L10N;
  this.toolbox = toolbox;
}

async function getNodeFront(gripOrFront, toolbox) {
  // Given a NodeFront
  if ("actorID" in gripOrFront) {
    return new Promise(resolve => resolve(gripOrFront));
  }

  const inspectorFront = await toolbox.target.getFront("inspector");
  return inspectorFront.getNodeFrontFromNodeGrip(gripOrFront);
}

DebuggerPanel.prototype = {
  open: async function() {
    const {
      actions,
      store,
      selectors,
      client,
    } = await this.panelWin.Debugger.bootstrap({
      threadFront: this.toolbox.threadFront,
      tabTarget: this.toolbox.target,
      debuggerClient: this.toolbox.target.client,
      workers: {
        sourceMaps: this.toolbox.sourceMapService,
        evaluationsParser: this.toolbox.parserService,
      },
      panel: this,
    });

    this._actions = actions;
    this._store = store;
    this._selectors = selectors;
    this._client = client;
    this.isReady = true;

    this.panelWin.document.addEventListener(
      "drag:start",
      this.toolbox.toggleDragging
    );
    this.panelWin.document.addEventListener(
      "drag:end",
      this.toolbox.toggleDragging
    );

    return this;
  },

  getVarsForTests() {
    return {
      store: this._store,
      selectors: this._selectors,
      actions: this._actions,
      client: this._client,
    };
  },

  _getState: function() {
    return this._store.getState();
  },

  getToolboxStore: function() {
    return this.toolbox.store;
  },

  openLink: function(url) {
    openContentLink(url);
  },

  openConsoleAndEvaluate: async function(input) {
    const { hud } = await this.toolbox.selectTool("webconsole");
    hud.ui.wrapper.dispatchEvaluateExpression(input);
  },

  openInspector: async function() {
    this.toolbox.selectTool("inspector");
  },

  openElementInInspector: async function(gripOrFront) {
    const onSelectInspector = this.toolbox.selectTool("inspector");
    const onGripNodeToFront = getNodeFront(gripOrFront, this.toolbox);

    const [front, inspector] = await Promise.all([
      onGripNodeToFront,
      onSelectInspector,
    ]);

    const onInspectorUpdated = inspector.once("inspector-updated");
    const onNodeFrontSet = this.toolbox.selection.setNodeFront(front, {
      reason: "debugger",
    });

    return Promise.all([onNodeFrontSet, onInspectorUpdated]);
  },

  highlightDomElement: async function(gripOrFront) {
    if (!this._highlight) {
      const { highlight, unhighlight } = this.toolbox.getHighlighter();
      this._highlight = highlight;
      this._unhighlight = unhighlight;
    }

    return this._highlight(gripOrFront);
  },

  unHighlightDomElement: function() {
    if (!this._unhighlight) {
      return;
    }

    const forceUnHighlightInTest = true;
    return this._unhighlight(forceUnHighlightInTest);
  },

  getFrames: function() {
    const thread = this._selectors.getCurrentThread(this._getState());
    const frames = this._selectors.getFrames(this._getState(), thread);

    // Frames is null when the debugger is not paused.
    if (!frames) {
      return {
        frames: [],
        selected: -1,
      };
    }

    const selectedFrame = this._selectors.getSelectedFrame(
      this._getState(),
      thread
    );
    const selected = frames.findIndex(frame => frame.id == selectedFrame.id);

    frames.forEach(frame => {
      frame.actor = frame.id;
    });
    const target = this._client.lookupTarget(thread);

    return { frames, selected, target };
  },

  getMappedExpression(expression) {
    return this._actions.getMappedExpression(expression);
  },

  isPaused() {
    const thread = this._selectors.getCurrentThread(this._getState());
    return this._selectors.getIsPaused(this._getState(), thread);
  },

  selectSourceURL(url, line, column) {
    const cx = this._selectors.getContext(this._getState());
    return this._actions.selectSourceURL(cx, url, { line, column });
  },

  async selectWorker(workerTargetFront) {
    const threadId = workerTargetFront.threadFront.actorID;
    const isThreadAvailable = this._selectors
      .getThreads(this._getState())
      .find(x => x.actor === threadId);

    if (!features.windowlessServiceWorkers) {
      console.error(
        "Selecting a worker needs the pref debugger.features.windowless-service-workers set to true"
      );
    } else if (!isThreadAvailable) {
      console.error(`Worker ${threadId} is not available for debugging`);
    } else {
      // select worker's thread
      const cx = this._selectors.getContext(this._getState());
      this._actions.selectThread(cx, threadId);

      // select worker's source
      const source = this.getSourceByURL(workerTargetFront._url);
      await this.selectSource(source.id, 1, 1);
    }
  },

  previewPausedLocation(location) {
    return this._actions.previewPausedLocation(location);
  },

  clearPreviewPausedLocation() {
    return this._actions.clearPreviewPausedLocation();
  },

  async selectSource(sourceId, line, column) {
    const cx = this._selectors.getContext(this._getState());
    const location = { sourceId, line, column };

    await this._actions.selectSource(cx, sourceId, location);
    if (this._selectors.hasLogpoint(this._getState(), location)) {
      this._actions.openConditionalPanel(location, true);
    }
  },

  canLoadSource(sourceId) {
    return this._selectors.canLoadSource(this._getState(), sourceId);
  },

  getSourceByActorId(sourceId) {
    return this._selectors.getSourceByActorId(this._getState(), sourceId);
  },

  getSourceByURL(sourceURL) {
    return this._selectors.getSourceByURL(this._getState(), sourceURL);
  },

  destroy: function() {
    this.panelWin.Debugger.destroy();
    this.emit("destroyed");
  },
};

exports.DebuggerPanel = DebuggerPanel;
