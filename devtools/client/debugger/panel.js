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
  "resource://devtools/client/shared/link.js",
  true
);
loader.lazyRequireGetter(
  this,
  "features",
  "resource://devtools/client/debugger/src/utils/prefs.js",
  true
);
loader.lazyRequireGetter(
  this,
  "getOriginalLocation",
  "resource://devtools/client/debugger/src/utils/source-maps.js",
  true
);
loader.lazyRequireGetter(
  this,
  "createLocation",
  "resource://devtools/client/debugger/src/utils/location.js",
  true
);
loader.lazyRequireGetter(
  this,
  "registerStoreObserver",
  "resource://devtools/client/shared/redux/subscriber.js",
  true
);
loader.lazyRequireGetter(
  this,
  "getMappedExpression",
  "resource://devtools/client/debugger/src/actions/expressions.js",
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

    const { actions, store, selectors, client } =
      await this.panelWin.Debugger.bootstrap({
        commands: this.commands,
        fluentBundles: fluentL10n.getBundles(),
        resourceCommand: this.toolbox.resourceCommand,
        workers: {
          sourceMapLoader: this.toolbox.sourceMapLoader,
          parserWorker: this.toolbox.parserWorker,
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

  async _onDebuggerStateChange(state, oldState) {
    const { getCurrentThread } = this._selectors;
    const currentThreadActorID = getCurrentThread(state);

    if (
      currentThreadActorID &&
      currentThreadActorID !== getCurrentThread(oldState)
    ) {
      const threadFront =
        this.commands.client.getFrontByID(currentThreadActorID);
      this.toolbox.selectTarget(threadFront?.targetFront.actorID);
    }

    this.toolbox.emit(
      "show-original-variable-mapping-warnings",
      this.shouldShowOriginalVariableMappingWarnings()
    );
  }

  shouldShowOriginalVariableMappingWarnings() {
    const { getSelectedSource, isMapScopesEnabled } = this._selectors;
    if (!this.isPaused() || isMapScopesEnabled(this._getState())) {
      return false;
    }
    const selectedSource = getSelectedSource(this._getState());
    return selectedSource?.isOriginal && !selectedSource?.isPrettyPrinted;
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
    const thread = this._selectors.getCurrentThread(this._getState());
    return getMappedExpression(expression, thread, {
      getState: this._store.getState,
      parserWorker: this.toolbox.parserWorker,
    });
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
    return this._actions.selectSourceURL(url, { line, column });
  }

  /**
   * This is called when some other panels wants to open a given source
   * in the debugger at a precise line/column.
   *
   * @param {String} generatedURL
   * @param {Number} generatedLine
   * @param {Number} generatedColumn
   * @param {String} sourceActorId (optional)
   *        If the callsite knows about a particular sourceActorId,
   *        or if the source doesn't have a URL, you have to pass a sourceActorId.
   * @param {String} reason
   *        A telemetry identifier to record when opening the debugger.
   *        This help differentiate why we opened the debugger.
   *
   * @return {Boolean}
   *         Returns true if the location is known by the debugger
   *         and the debugger opens it.
   */
  async openSourceInDebugger({
    generatedURL,
    generatedLine,
    generatedColumn,
    sourceActorId,
    reason,
  }) {
    const generatedSource = sourceActorId
      ? this._selectors.getSourceByActorId(this._getState(), sourceActorId)
      : this._selectors.getSourceByURL(this._getState(), generatedURL);
    // We won't try opening source in the debugger when we can't find the related source actor in the reducer,
    // or, when it doesn't have any related source actor registered.
    if (
      !generatedSource ||
      // Note: We're not entirely sure when this can happen,
      // so we may want to revisit that at some point.
      !this._selectors.getSourceActorsForSource(
        this._getState(),
        generatedSource.id
      ).length
    ) {
      return false;
    }

    const generatedLocation = createLocation({
      source: generatedSource,
      line: generatedLine,
      column: generatedColumn,
    });

    // Note that getOriginalLocation can easily return generatedLocation
    // if the location can't be mapped to any original source.
    // So that we may open either regular source or original sources here.
    const originalLocation = await getOriginalLocation(generatedLocation, {
      // Reproduce a minimal thunkArgs for getOriginalLocation.
      sourceMapLoader: this.toolbox.sourceMapLoader,
      getState: this._store.getState,
    });

    // view-source module only forced the load of debugger in the background.
    // Now that we know we want to show a source, force displaying it in foreground.
    //
    // Note that browser_markup_view-source.js doesn't wait for the debugger
    // to be fully loaded with the source and requires the debugger to be loaded late.
    // But we might try to load display it early to improve user perception.
    await this.toolbox.selectTool("jsdebugger", reason);

    await this._actions.selectSpecificLocation(originalLocation);

    // XXX: should this be moved to selectSpecificLocation??
    if (this._selectors.hasLogpoint(this._getState(), originalLocation)) {
      this._actions.openConditionalPanel(originalLocation, true);
    }

    return true;
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
    const source = this._selectors.getSourceByURL(
      this._getState(),
      workerDescriptorFront._url
    );
    const sourceActor = this._selectors.getFirstSourceActorForGeneratedSource(
      this._getState(),
      source.id,
      threadActorID
    );
    await this._actions.selectSource(source, sourceActor);
  }

  selectThread(threadActorID) {
    this._actions.selectThread(threadActorID);
  }

  toggleJavascriptTracing() {
    this._actions.toggleTracing(
      this._selectors.getJavascriptTracingLogMethod(this._getState())
    );
  }

  destroy() {
    this.panelWin.Debugger.destroy();
    this.emit("destroyed");
  }
}

exports.DebuggerPanel = DebuggerPanel;
