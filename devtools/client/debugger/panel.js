/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const { MultiLocalizationHelper } = require("devtools/shared/l10n");
const {
  FluentL10n,
} = require("devtools/client/shared/fluent-l10n/fluent-l10n");

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
loader.lazyRequireGetter(
  this,
  "registerStoreObserver",
  "devtools/client/shared/redux/subscriber",
  true
);

const DBG_STRINGS_URI = [
  "devtools/client/locales/debugger.properties",
  // These are used in the AppErrorBoundary component
  "devtools/client/locales/startup.properties",
  "devtools/client/locales/components.properties",
];
const L10N = new MultiLocalizationHelper(...DBG_STRINGS_URI);

async function getNodeFront(gripOrFront, toolbox) {
  // Given a NodeFront
  if ("actorID" in gripOrFront) {
    return new Promise(resolve => resolve(gripOrFront));
  }

  const inspectorFront = await toolbox.target.getFront("inspector");
  return inspectorFront.getNodeFrontFromNodeGrip(gripOrFront);
}

class DebuggerPanel {
  constructor(iframeWindow, toolbox, commands) {
    this.panelWin = iframeWindow;
    this.panelWin.L10N = L10N;

    this.toolbox = toolbox;
    this.commands = commands;
  }

  async open() {
    // whypaused-* strings are in devtools/shared as they're used in the PausedDebuggerOverlay as well
    const fluentL10n = new FluentL10n();
    await fluentL10n.init(["devtools/shared/debugger-paused-reasons.ftl"]);

    const {
      actions,
      store,
      selectors,
      client,
    } = await this.panelWin.Debugger.bootstrap({
      commands: this.commands,
      fluentBundles: fluentL10n.getBundles(),
      resourceCommand: this.toolbox.resourceCommand,
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

    registerStoreObserver(this._store, this._onDebuggerStateChange.bind(this));

    return this;
  }

  _onDebuggerStateChange(state, oldState) {
    const { getCurrentThread } = this._selectors;

    const currentThreadActorID = getCurrentThread(state);
    if (
      currentThreadActorID &&
      currentThreadActorID !== getCurrentThread(oldState)
    ) {
      const threadFront = this.commands.client.getFrontByID(
        currentThreadActorID
      );
      this.toolbox.selectTarget(threadFront?.targetFront.actorID);
    }
  }

  getVarsForTests() {
    return {
      store: this._store,
      selectors: this._selectors,
      actions: this._actions,
      client: this._client,
    };
  }

  _getState() {
    return this._store.getState();
  }

  getToolboxStore() {
    return this.toolbox.store;
  }

  openLink(url) {
    openContentLink(url);
  }

  async openConsoleAndEvaluate(input) {
    const { hud } = await this.toolbox.selectTool("webconsole");
    hud.ui.wrapper.dispatchEvaluateExpression(input);
  }

  async openInspector() {
    this.toolbox.selectTool("inspector");
  }

  async openElementInInspector(gripOrFront) {
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
  }

  highlightDomElement(gripOrFront) {
    if (!this._highlight) {
      const { highlight, unhighlight } = this.toolbox.getHighlighter();
      this._highlight = highlight;
      this._unhighlight = unhighlight;
    }

    return this._highlight(gripOrFront);
  }

  unHighlightDomElement() {
    if (!this._unhighlight) {
      return Promise.resolve();
    }

    return this._unhighlight();
  }

  /**
   * Return the Frame Actor ID of the currently selected frame,
   * or null if the debugger isn't paused.
   */
  getSelectedFrameActorID() {
    const thread = this._selectors.getCurrentThread(this._getState());
    const selectedFrame = this._selectors.getSelectedFrame(
      this._getState(),
      thread
    );
    if (selectedFrame) {
      return selectedFrame.id;
    }
    return null;
  }

  getMappedExpression(expression) {
    return this._actions.getMappedExpression(expression);
  }

  /**
   * Return the source-mapped variables for the current scope.
   * @returns {{[String]: String} | null} A dictionary mapping original variable names to generated
   * variable names if map scopes is enabled, otherwise null.
   */
  getMappedVariables() {
    if (!this._selectors.isMapScopesEnabled(this._getState())) {
      return null;
    }
    const thread = this._selectors.getCurrentThread(this._getState());
    return this._selectors.getSelectedScopeMappings(this._getState(), thread);
  }

  isPaused() {
    const thread = this._selectors.getCurrentThread(this._getState());
    return this._selectors.getIsPaused(this._getState(), thread);
  }

  selectSourceURL(url, line, column) {
    const cx = this._selectors.getContext(this._getState());
    return this._actions.selectSourceURL(cx, url, { line, column });
  }

  async selectWorker(workerDescriptorFront) {
    const threadActorID = workerDescriptorFront.threadFront?.actorID;

    const isThreadAvailable = this._selectors
      .getThreads(this._getState())
      .find(x => x.actor === threadActorID);

    if (!features.windowlessServiceWorkers) {
      console.error(
        "Selecting a worker needs the pref debugger.features.windowless-service-workers set to true"
      );
      return;
    }

    if (!isThreadAvailable) {
      console.error(`Worker ${threadActorID} is not available for debugging`);
      return;
    }

    // select worker's thread
    this.selectThread(threadActorID);

    // select worker's source
    const source = this.getSourceByURL(workerDescriptorFront._url);
    await this.selectSource(source.id, 1, 1);
  }

  selectThread(threadActorID) {
    const cx = this._selectors.getContext(this._getState());
    this._actions.selectThread(cx, threadActorID);
  }

  async selectSource(sourceId, line, column) {
    const cx = this._selectors.getContext(this._getState());
    const location = { sourceId, line, column };

    await this._actions.selectSource(cx, sourceId, location);
    if (this._selectors.hasLogpoint(this._getState(), location)) {
      this._actions.openConditionalPanel(location, true);
    }
  }

  getSourceActorsForSource(sourceId) {
    return this._selectors.getSourceActorsForSource(this._getState(), sourceId);
  }

  getSourceByActorId(sourceId) {
    return this._selectors.getSourceByActorId(this._getState(), sourceId);
  }

  getSourceByURL(sourceURL) {
    return this._selectors.getSourceByURL(this._getState(), sourceURL);
  }

  getSource(sourceId) {
    return this._selectors.getSource(this._getState(), sourceId);
  }

  getLocationSource(location) {
    return this._selectors.getLocationSource(this._getState(), location);
  }

  destroy() {
    this.panelWin.Debugger.destroy();
    this.emit("destroyed");
  }
}

exports.DebuggerPanel = DebuggerPanel;
