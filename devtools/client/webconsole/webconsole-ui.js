/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EventEmitter = require("devtools/shared/event-emitter");
const KeyShortcuts = require("devtools/client/shared/key-shortcuts");
const { l10n } = require("devtools/client/webconsole/utils/messages");

const { BrowserLoader } = ChromeUtils.import(
  "resource://devtools/shared/loader/browser-loader.js"
);
const {
  getAdHocFrontOrPrimitiveGrip,
} = require("devtools/client/fronts/object");

const { PREFS } = require("devtools/client/webconsole/constants");

const FirefoxDataProvider = require("devtools/client/netmonitor/src/connector/firefox-data-provider");

loader.lazyRequireGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm",
  true
);
loader.lazyRequireGetter(
  this,
  "constants",
  "devtools/client/webconsole/constants"
);

loader.lazyRequireGetter(
  this,
  "START_IGNORE_ACTION",
  "devtools/client/shared/redux/middleware/ignore",
  true
);
const ZoomKeys = require("devtools/client/shared/zoom-keys");

const PREF_SIDEBAR_ENABLED = "devtools.webconsole.sidebarToggle";
const PREF_BROWSERTOOLBOX_SCOPE = "devtools.browsertoolbox.scope";

/**
 * A WebConsoleUI instance is an interactive console initialized *per target*
 * that displays console log data as well as provides an interactive terminal to
 * manipulate the target's document content.
 *
 * The WebConsoleUI is responsible for the actual Web Console UI
 * implementation.
 */
class WebConsoleUI {
  /*
   * @param {WebConsole} hud: The WebConsole owner object.
   */
  constructor(hud) {
    this.hud = hud;
    this.hudId = this.hud.hudId;
    this.isBrowserConsole = this.hud.isBrowserConsole;

    this.isBrowserToolboxConsole =
      this.hud.commands.descriptorFront.isBrowserProcessDescriptor &&
      !this.isBrowserConsole;
    this.fissionSupport = Services.prefs.getBoolPref(
      constants.PREFS.FEATURES.BROWSER_TOOLBOX_FISSION
    );

    this.window = this.hud.iframeWindow;

    this._onPanelSelected = this._onPanelSelected.bind(this);
    this._onChangeSplitConsoleState = this._onChangeSplitConsoleState.bind(
      this
    );
    this._onTargetAvailable = this._onTargetAvailable.bind(this);
    this._onTargetDestroyed = this._onTargetDestroyed.bind(this);
    this._onResourceAvailable = this._onResourceAvailable.bind(this);
    this._onNetworkResourceUpdated = this._onNetworkResourceUpdated.bind(this);
    this.clearPrivateMessages = this.clearPrivateMessages.bind(this);
    this._onScopePrefChanged = this._onScopePrefChanged.bind(this);

    if (this.isBrowserConsole) {
      Services.prefs.addObserver(
        PREF_BROWSERTOOLBOX_SCOPE,
        this._onScopePrefChanged
      );
    }

    EventEmitter.decorate(this);
  }

  /**
   * Getter for the WebConsoleFront.
   * @type object
   */
  get webConsoleFront() {
    return this._webConsoleFront;
  }

  /**
   * Initialize the WebConsoleUI instance.
   * @return object
   *         A promise object that resolves once the frame is ready to use.
   */
  init() {
    if (this._initializer) {
      return this._initializer;
    }

    this._initializer = (async () => {
      this._initUI();

      if (this.isBrowserConsole) {
        // Bug 1605763:
        // TargetCommand.startListening will start fetching additional targets
        // and may overload the Browser Console with loads of targets and resources.
        // We can call it from here, as `_attachTargets` is called after the UI is initialized.
        // Bug 1642599:
        // TargetCommand.startListening has to be called before:
        // - `_attachTargets`, in order to set TargetCommand.watcherFront which is used by ResourceWatcher.watchResources.
        // - `ConsoleCommands`, in order to set TargetCommand.targetFront which is wrapped by hud.currentTarget
        await this.hud.commands.targetCommand.startListening();
      }

      await this.wrapper.init();

      // Bug 1605763: It's important to call _attachTargets once the UI is initialized, as
      // it may overload the Browser Console with many updates.
      // It is also important to do it only after the wrapper is initialized,
      // otherwise its `store` will be null while we already call a few dispatch methods
      // from onResourceAvailable
      await this._attachTargets();

      // `_attachTargets` will process resources and throttle some actions
      // Wait for these actions to be dispatched before reporting that the
      // console is initialized. Otherwise `showToolbox` will resolve before
      // all already existing console messages are displayed.
      await this.wrapper.waitAsyncDispatches();
    })();

    return this._initializer;
  }

  destroy() {
    if (this._destroyed) {
      return;
    }

    this._destroyed = true;

    this.React = this.ReactDOM = this.FrameView = null;

    if (this.wrapper) {
      this.wrapper.getStore().dispatch(START_IGNORE_ACTION);
      this.wrapper.destroy();
    }

    if (this.jsterm) {
      this.jsterm.destroy();
      this.jsterm = null;
    }

    const { toolbox } = this.hud;
    if (toolbox) {
      toolbox.off("webconsole-selected", this._onPanelSelected);
      toolbox.off("split-console", this._onChangeSplitConsoleState);
      toolbox.off("select", this._onChangeSplitConsoleState);
    }

    if (this.isBrowserConsole) {
      Services.prefs.removeObserver(
        PREF_BROWSERTOOLBOX_SCOPE,
        this._onScopePrefChanged
      );
    }

    // Stop listening for targets
    this.hud.commands.targetCommand.unwatchTargets({
      types: this.hud.commands.targetCommand.ALL_TYPES,
      onAvailable: this._onTargetAvailable,
      onDestroyed: this._onTargetDestroyed,
    });

    const resourceCommand = this.hud.resourceCommand;
    resourceCommand.unwatchResources(
      [
        resourceCommand.TYPES.CONSOLE_MESSAGE,
        resourceCommand.TYPES.ERROR_MESSAGE,
        resourceCommand.TYPES.PLATFORM_MESSAGE,
        resourceCommand.TYPES.CLONED_CONTENT_PROCESS_MESSAGE,
        resourceCommand.TYPES.DOCUMENT_EVENT,
      ],
      { onAvailable: this._onResourceAvailable }
    );
    resourceCommand.unwatchResources([resourceCommand.TYPES.CSS_MESSAGE], {
      onAvailable: this._onResourceAvailable,
    });

    this.stopWatchingNetworkResources();

    this._webConsoleFront = null;
    this.networkDataProvider.destroy();
    this.networkDataProvider = null;

    // Nullify `hud` last as it nullify also target which is used on destroy
    this.window = this.hud = this.wrapper = null;
  }

  /**
   * Clear the Web Console output.
   *
   * This method emits the "messages-cleared" notification.
   *
   * @param boolean clearStorage
   *        True if you want to clear the console messages storage associated to
   *        this Web Console.
   * @param object event
   *        If the event exists, calls preventDefault on it.
   */
  async clearOutput(clearStorage, event) {
    if (event) {
      event.preventDefault();
    }
    if (this.wrapper) {
      this.wrapper.dispatchMessagesClear();
    }

    if (clearStorage) {
      await this.clearMessagesCache();
    }
    this.emitForTests("messages-cleared");
  }

  async clearMessagesCache() {
    if (this._destroyed) {
      return;
    }

    // This can be called during console destruction and getAllFronts would reject in such case.
    try {
      const consoleFronts = await this.hud.commands.targetCommand.getAllFronts(
        this.hud.commands.targetCommand.ALL_TYPES,
        "console"
      );
      const promises = [];
      for (const consoleFront of consoleFronts) {
        promises.push(consoleFront.clearMessagesCacheAsync());
      }
      await Promise.all(promises);
      this.emitForTests("messages-cache-cleared");
    } catch (e) {
      console.warn("Exception in clearMessagesCache", e);
    }
  }

  /**
   * Remove all of the private messages from the Web Console output.
   *
   * This method emits the "private-messages-cleared" notification.
   */
  clearPrivateMessages() {
    if (this._destroyed) {
      return;
    }

    this.wrapper.dispatchPrivateMessagesClear();
    this.emitForTests("private-messages-cleared");
  }

  inspectObjectActor(objectActor) {
    const { webConsoleFront } = this;
    this.wrapper.dispatchMessageAdd(
      {
        helperResult: {
          type: "inspectObject",
          object:
            objectActor && objectActor.getGrip
              ? objectActor
              : getAdHocFrontOrPrimitiveGrip(objectActor, webConsoleFront),
        },
      },
      true
    );
    return this.wrapper;
  }

  disableAllNetworkMessages() {
    if (this._destroyed) {
      return;
    }
    this.wrapper.dispatchNetworkMessagesDisable();
  }

  getPanelWindow() {
    return this.window;
  }

  logWarningAboutReplacedAPI() {
    return this.hud.currentTarget.logWarningInPage(
      l10n.getStr("ConsoleAPIDisabled"),
      "ConsoleAPIDisabled"
    );
  }

  /**
   * Connect to the server using the remote debugging protocol.
   *
   * @private
   * @return object
   *         A promise object that is resolved/reject based on the proxies connections.
   */
  async _attachTargets() {
    const { commands, resourceCommand } = this.hud;
    this.networkDataProvider = new FirefoxDataProvider({
      commands,
      actions: {
        updateRequest: (id, data) =>
          this.wrapper.batchedRequestUpdates({ id, data }),
      },
      owner: this,
    });

    // Listen for all target types, including:
    // - frames, in order to get the parent process target
    // which is considered as a frame rather than a process.
    // - workers, for similar reason. When we open a toolbox
    // for just a worker, the top level target is a worker target.
    // - processes, as we want to spawn additional proxies for them.
    await commands.targetCommand.watchTargets({
      types: this.hud.commands.targetCommand.ALL_TYPES,
      onAvailable: this._onTargetAvailable,
      onDestroyed: this._onTargetDestroyed,
    });

    await resourceCommand.watchResources(
      [
        resourceCommand.TYPES.CONSOLE_MESSAGE,
        resourceCommand.TYPES.ERROR_MESSAGE,
        resourceCommand.TYPES.PLATFORM_MESSAGE,
        resourceCommand.TYPES.CLONED_CONTENT_PROCESS_MESSAGE,
        resourceCommand.TYPES.DOCUMENT_EVENT,
      ],
      { onAvailable: this._onResourceAvailable }
    );

    if (this.isBrowserConsole || this.isBrowserToolboxConsole) {
      const shouldEnableNetworkMonitoring = Services.prefs.getBoolPref(
        PREFS.UI.ENABLE_NETWORK_MONITORING
      );
      if (shouldEnableNetworkMonitoring) {
        await this.startWatchingNetworkResources();
      } else {
        await this.stopWatchingNetworkResources();
      }
    } else {
      // We should always watch for network resources in the webconsole
      await this.startWatchingNetworkResources();
    }
  }

  async startWatchingNetworkResources() {
    const { commands, resourceCommand } = this.hud;
    await resourceCommand.watchResources(
      [
        resourceCommand.TYPES.NETWORK_EVENT,
        resourceCommand.TYPES.NETWORK_EVENT_STACKTRACE,
      ],
      {
        onAvailable: this._onResourceAvailable,
        onUpdated: this._onNetworkResourceUpdated,
      }
    );

    // Until we enable NETWORK_EVENT server watcher in the browser toolbox
    // we still have to support the console actor codepath.
    const hasNetworkResourceCommandSupport = resourceCommand.hasResourceCommandSupport(
      resourceCommand.TYPES.NETWORK_EVENT
    );
    const supportsWatcherRequest = commands.targetCommand.hasTargetWatcherSupport();
    if (hasNetworkResourceCommandSupport && supportsWatcherRequest) {
      const networkFront = await commands.watcherFront.getNetworkParentActor();
      //
      // There is no way to view response bodies from the Browser Console, so do
      // not waste the memory.
      const saveBodies =
        !this.isBrowserConsole &&
        Services.prefs.getBoolPref(
          "devtools.netmonitor.saveRequestAndResponseBodies"
        );
      await networkFront.setSaveRequestAndResponseBodies(saveBodies);
    }
  }

  async stopWatchingNetworkResources() {
    if (this._destroyed) {
      return;
    }

    await this.hud.resourceCommand.unwatchResources(
      [
        this.hud.resourceCommand.TYPES.NETWORK_EVENT,
        this.hud.resourceCommand.TYPES.NETWORK_EVENT_STACKTRACE,
      ],
      {
        onAvailable: this._onResourceAvailable,
        onUpdated: this._onNetworkResourceUpdated,
      }
    );
  }

  handleDocumentEvent(resource) {
    // Only consider top level document, and ignore remote iframes top document
    if (!resource.targetFront.isTopLevel) {
      return;
    }

    if (resource.name == "will-navigate") {
      this.handleWillNavigate({
        timeStamp: resource.time,
        url: resource.newURI,
      });
    } else if (resource.name == "dom-complete") {
      this.handleNavigated({
        hasNativeConsoleAPI: resource.hasNativeConsoleAPI,
      });
    }
    // For now, ignore all other DOCUMENT_EVENT's.
  }

  /**
   * Handler for when the page is done loading.
   *
   * @param Boolean hasNativeConsoleAPI
   *        True if the `console` object is the native one and hasn't been overloaded by a custom
   *        object by the page itself.
   */
  async handleNavigated({ hasNativeConsoleAPI }) {
    // Updates instant evaluation on page navigation
    this.wrapper.dispatchUpdateInstantEvaluationResultForCurrentExpression();

    // Wait for completion of any async dispatch before notifying that the console
    // is fully updated after a page reload
    await this.wrapper.waitAsyncDispatches();

    if (!hasNativeConsoleAPI) {
      this.logWarningAboutReplacedAPI();
    }

    this.emit("reloaded");
  }

  handleWillNavigate({ timeStamp, url }) {
    this.wrapper.dispatchTabWillNavigate({ timeStamp, url });
  }

  async watchCssMessages() {
    const { resourceCommand } = this.hud;
    await resourceCommand.watchResources([resourceCommand.TYPES.CSS_MESSAGE], {
      onAvailable: this._onResourceAvailable,
    });
  }

  _onResourceAvailable(resources) {
    if (this._destroyed) {
      return;
    }

    const messages = [];
    for (const resource of resources) {
      const { TYPES } = this.hud.resourceCommand;
      if (resource.resourceType === TYPES.DOCUMENT_EVENT) {
        this.handleDocumentEvent(resource);
        continue;
      }
      // Ignore messages forwarded from content processes if we're in fission browser toolbox.
      if (
        !this.wrapper ||
        ((resource.resourceType === TYPES.ERROR_MESSAGE ||
          resource.resourceType === TYPES.CSS_MESSAGE) &&
          resource.pageError?.isForwardedFromContentProcess &&
          (this.isBrowserToolboxConsole || this.isBrowserConsole) &&
          this.fissionSupport)
      ) {
        continue;
      }

      // Don't show messages emitted from a private window before the Browser Console was
      // opened to avoid leaking data from past usage of the browser (e.g. content message
      // from now closed private tabs)
      if (
        (this.isBrowserToolboxConsole || this.isBrowserConsole) &&
        resource.isAlreadyExistingResource &&
        (resource.pageError?.private || resource.message?.private)
      ) {
        continue;
      }

      if (resource.resourceType === TYPES.NETWORK_EVENT_STACKTRACE) {
        this.networkDataProvider?.onStackTraceAvailable(resource);
        continue;
      }

      if (resource.resourceType === TYPES.NETWORK_EVENT) {
        this.networkDataProvider?.onNetworkResourceAvailable(resource);
      }
      messages.push(resource);
    }
    this.wrapper.dispatchMessagesAdd(messages);
  }

  _onNetworkResourceUpdated(updates) {
    if (this._destroyed) {
      return;
    }

    const messageUpdates = [];
    for (const { resource } of updates) {
      if (
        resource.resourceType == this.hud.resourceCommand.TYPES.NETWORK_EVENT
      ) {
        this.networkDataProvider?.onNetworkResourceUpdated(resource);
        messageUpdates.push(resource);
      }
    }
    this.wrapper.dispatchMessagesUpdate(messageUpdates);
  }

  /**
   * Called any time a new target is available.
   * i.e. it was already existing or has just been created.
   *
   * @private
   * @param Front targetFront
   *        The Front of the target that is available.
   *        This Front inherits from TargetMixin and is typically
   *        composed of a WindowGlobalTargetFront or ContentProcessTargetFront.
   */
  async _onTargetAvailable({ targetFront }) {
    if (this._destroyed) {
      return;
    }

    // Once we support only server watcher for NETWORK_EVENT, we will be able to drop this.
    // We have to wait for the fully enabling of NETWORK_EVENT watchers, especially on the Browser Toolbox.
    const { targetCommand, resourceCommand } = this.hud.commands;
    const hasNetworkResourceCommandSupport = resourceCommand.hasResourceCommandSupport(
      resourceCommand.TYPES.NETWORK_EVENT
    );
    const supportsWatcherRequest = targetCommand.hasTargetWatcherSupport();
    if (!hasNetworkResourceCommandSupport || !supportsWatcherRequest) {
      // There is no way to view response bodies from the Browser Console, so do
      // not waste the memory.
      const saveBodies =
        !this.isBrowserConsole &&
        Services.prefs.getBoolPref(
          "devtools.netmonitor.saveRequestAndResponseBodies"
        );

      const front = await targetFront.getFront("console");
      // Make sure the web console client connection is established first.
      await front.setPreferences({
        "NetworkMonitor.saveRequestAndResponseBodies": saveBodies,
      });
    }

    if (targetFront.isTopLevel) {
      this._webConsoleFront = await targetFront.getFront("console");
      if (this.isBrowserConsole || this.isBrowserToolboxConsole) {
        // Private messages only need to be removed from the output in Browser Console/Browser Toolbox
        this.webConsoleFront.on(
          "lastPrivateContextExited",
          this.clearPrivateMessages
        );
      }
    }
  }

  _onTargetDestroyed({ targetFront, isModeSwitching }) {
    // Don't try to do anything if the WebConsole is being destroyed
    if (this._destroyed) {
      return;
    }

    // We only want to remove messages from a target destroyed when we're switching mode
    // in the Browser Console/Browser Toolbox Console.
    // For regular cases, we want to keep the message history (the output will still be
    // cleared when the top level target navigates, if "Persist Logs" isn't true, via handleWillNavigate)
    if (isModeSwitching) {
      this.wrapper.dispatchTargetMessagesRemove(targetFront);
    }
  }

  _initUI() {
    this.document = this.window.document;
    this.rootElement = this.document.documentElement;

    this.outputNode = this.document.getElementById("app-wrapper");

    const { toolbox } = this.hud;

    // Initialize module loader and load all the WebConsoleWrapper. The entire code-base
    // doesn't need any extra privileges and runs entirely in content scope.
    const WebConsoleWrapper = BrowserLoader({
      baseURI: "resource://devtools/client/webconsole/",
      window: this.window,
    }).require("devtools/client/webconsole/webconsole-wrapper");

    this.wrapper = new WebConsoleWrapper(
      this.outputNode,
      this,
      toolbox,
      this.document
    );

    this._initShortcuts();
    this._initOutputSyntaxHighlighting();

    if (toolbox) {
      toolbox.on("webconsole-selected", this._onPanelSelected);
      toolbox.on("split-console", this._onChangeSplitConsoleState);
      toolbox.on("select", this._onChangeSplitConsoleState);
    }
  }

  _initOutputSyntaxHighlighting() {
    // Given a DOM node, we syntax highlight identically to how the input field
    // looks. See https://codemirror.net/demo/runmode.html;
    const syntaxHighlightNode = node => {
      const editor = this.jsterm && this.jsterm.editor;
      if (node && editor) {
        node.classList.add("cm-s-mozilla");
        editor.CodeMirror.runMode(
          node.textContent,
          "application/javascript",
          node
        );
      }
    };

    // Use a Custom Element to handle syntax highlighting to avoid
    // dealing with refs or innerHTML from React.
    const win = this.window;
    win.customElements.define(
      "syntax-highlighted",
      class extends win.HTMLElement {
        connectedCallback() {
          if (!this.connected) {
            this.connected = true;
            syntaxHighlightNode(this);

            // Highlight Again when the innerText changes
            // We remove the listener before running codemirror mode and add
            // it again to capture text changes
            this.observer = new win.MutationObserver((mutations, observer) => {
              observer.disconnect();
              syntaxHighlightNode(this);
              observer.observe(this, { childList: true });
            });

            this.observer.observe(this, { childList: true });
          }
        }
      }
    );
  }

  _initShortcuts() {
    const shortcuts = new KeyShortcuts({
      window: this.window,
    });

    let clearShortcut;
    if (AppConstants.platform === "macosx") {
      const alternativaClearShortcut = l10n.getStr(
        "webconsole.clear.alternativeKeyOSX"
      );
      shortcuts.on(alternativaClearShortcut, event =>
        this.clearOutput(true, event)
      );
      clearShortcut = l10n.getStr("webconsole.clear.keyOSX");
    } else {
      clearShortcut = l10n.getStr("webconsole.clear.key");
    }

    shortcuts.on(clearShortcut, event => this.clearOutput(true, event));

    if (this.isBrowserConsole) {
      // Make sure keyboard shortcuts work immediately after opening
      // the Browser Console (Bug 1461366).
      this.window.focus();
      shortcuts.on(
        l10n.getStr("webconsole.close.key"),
        this.window.close.bind(this.window)
      );

      ZoomKeys.register(this.window, shortcuts);

      /* This is the same as DevelopmentHelpers.quickRestart, but it runs in all
       * builds (even official). This allows a user to do a restart + session restore
       * with Ctrl+Shift+J (open Browser Console) and then Ctrl+Alt+R (restart).
       */
      shortcuts.on("CmdOrCtrl+Alt+R", () => {
        this.hud.commands.targetCommand.reloadTopLevelTarget();
      });
    } else if (Services.prefs.getBoolPref(PREF_SIDEBAR_ENABLED)) {
      shortcuts.on("Esc", event => {
        this.wrapper.dispatchSidebarClose();
        if (this.jsterm) {
          this.jsterm.focus();
        }
      });
    }
  }

  /**
   * Sets the focus to JavaScript input field when the web console tab is
   * selected or when there is a split console present.
   * @private
   */
  _onPanelSelected() {
    // We can only focus when we have the jsterm reference. This is fine because if the
    // jsterm is not mounted yet, it will be focused in JSTerm's componentDidMount.
    if (this.jsterm) {
      this.jsterm.focus();
    }
  }

  _onChangeSplitConsoleState() {
    this.wrapper.dispatchSplitConsoleCloseButtonToggle();
  }

  _onScopePrefChanged() {
    if (this.isBrowserConsole) {
      this.hud.updateWindowTitle();
    }
  }

  getInputCursor() {
    return this.jsterm && this.jsterm.getSelectionStart();
  }

  getJsTermTooltipAnchor() {
    return this.outputNode.querySelector(".CodeMirror-cursor");
  }

  attachRef(id, node) {
    this[id] = node;
  }

  getSelectedNodeActorID() {
    const inspectorSelection = this.hud.getInspectorSelection();
    return inspectorSelection?.nodeFront?.actorID;
  }
}

exports.WebConsoleUI = WebConsoleUI;
