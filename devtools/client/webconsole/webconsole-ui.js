/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Utils: WebConsoleUtils } = require("devtools/client/webconsole/utils");
const EventEmitter = require("devtools/shared/event-emitter");
const Services = require("Services");
const {
  WebConsoleConnectionProxy,
} = require("devtools/client/webconsole/webconsole-connection-proxy");
const KeyShortcuts = require("devtools/client/shared/key-shortcuts");
const { l10n } = require("devtools/client/webconsole/utils/messages");

var ChromeUtils = require("ChromeUtils");
const { BrowserLoader } = ChromeUtils.import(
  "resource://devtools/client/shared/browser-loader.js"
);

loader.lazyRequireGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm",
  true
);

const ZoomKeys = require("devtools/client/shared/zoom-keys");

const PREF_SIDEBAR_ENABLED = "devtools.webconsole.sidebarToggle";

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
    this.fissionSupport = this.hud.fissionSupport;
    this.window = this.hud.iframeWindow;

    this._onPanelSelected = this._onPanelSelected.bind(this);
    this._onChangeSplitConsoleState = this._onChangeSplitConsoleState.bind(
      this
    );

    EventEmitter.decorate(this);
  }

  /**
   * Getter for the debugger WebConsoleClient.
   * @type object
   */
  get webConsoleClient() {
    const proxy = this.getProxy();

    if (!proxy) {
      return null;
    }

    return proxy.webConsoleClient;
  }

  /**
   * Return the main target proxy, i.e. the proxy for MainProcessTarget in BrowserConsole,
   * and the proxy for the target passed from the Toolbox to WebConsole.
   *
   * @returns {WebConsoleConnectionProxy}
   */
  getProxy() {
    return this.proxy;
  }

  /**
   * Return all the proxies we're currently managing (i.e. the "main" one, and the
   * possible additional ones).
   *
   * @returns {Array<WebConsoleConnectionProxy>}
   */
  getAllProxies() {
    let proxies = [this.getProxy()];

    if (this.additionalProxies) {
      proxies = proxies.concat(this.additionalProxies);
    }

    return proxies;
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
      await this._initConnection();
      await this.wrapper.init();

      const id = WebConsoleUtils.supportsString(this.hudId);
      if (Services.obs) {
        Services.obs.notifyObservers(id, "web-console-created");
      }
    })();

    return this._initializer;
  }

  destroy() {
    if (!this.hud) {
      return;
    }

    this.React = this.ReactDOM = this.FrameView = null;

    if (this.outputNode) {
      // We do this because it's much faster than letting React handle the ConsoleOutput
      // unmounting.
      this.outputNode.innerHTML = "";
    }

    if (this.jsterm) {
      this.jsterm.destroy();
      this.jsterm = null;
    }

    if (this.wrapper) {
      this.wrapper.destroy();
    }

    const toolbox = this.hud.toolbox;
    if (toolbox) {
      toolbox.off("webconsole-selected", this._onPanelSelected);
      toolbox.off("split-console", this._onChangeSplitConsoleState);
      toolbox.off("select", this._onChangeSplitConsoleState);
    }

    for (const proxy of this.getAllProxies()) {
      proxy.disconnect();
    }
    this.proxy = null;
    this.additionalProxies = null;

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
  clearOutput(clearStorage, event) {
    if (event) {
      event.preventDefault();
    }
    if (this.wrapper) {
      this.wrapper.dispatchMessagesClear();
    }
    this.clearNetworkRequests();
    if (clearStorage) {
      this.clearMessagesCache();
    }
    this.emit("messages-cleared");
  }

  clearNetworkRequests() {
    for (const proxy of this.getAllProxies()) {
      proxy.webConsoleClient.clearNetworkRequests();
    }
  }

  clearMessagesCache() {
    for (const proxy of this.getAllProxies()) {
      proxy.webConsoleClient.clearMessagesCache();
    }
  }

  /**
   * Remove all of the private messages from the Web Console output.
   *
   * This method emits the "private-messages-cleared" notification.
   */
  clearPrivateMessages() {
    if (this.wrapper) {
      this.wrapper.dispatchPrivateMessagesClear();
      this.emit("private-messages-cleared");
    }
  }

  inspectObjectActor(objectActor) {
    this.wrapper.dispatchMessageAdd(
      {
        helperResult: {
          type: "inspectObject",
          object: objectActor,
        },
      },
      true
    );
    return this.wrapper;
  }

  logWarningAboutReplacedAPI() {
    return this.hud.target.logWarningInPage(
      l10n.getStr("ConsoleAPIDisabled"),
      "ConsoleAPIDisabled"
    );
  }

  /**
   * Setter for saving of network request and response bodies.
   *
   * @param boolean value
   *        The new value you want to set.
   */
  async setSaveRequestAndResponseBodies(value) {
    if (!this.webConsoleClient) {
      // Don't continue if the webconsole disconnected.
      return null;
    }

    const newValue = !!value;
    const toSet = {
      "NetworkMonitor.saveRequestAndResponseBodies": newValue,
    };

    // Make sure the web console client connection is established first.
    return this.webConsoleClient.setPreferences(toSet);
  }

  /**
   * Connect to the server using the remote debugging protocol.
   *
   * @private
   * @return object
   *         A promise object that is resolved/reject based on the proxies connections.
   */
  async _initConnection() {
    this.proxy = new WebConsoleConnectionProxy(this, this.hud.target);

    if (
      this.fissionSupport &&
      this.hud.target.chrome &&
      !this.hud.target.isAddon
    ) {
      const { mainRoot } = this.hud.target.client;
      const { processes } = await mainRoot.listProcesses();

      this.additionalProxies = [];
      for (const processDescriptor of processes) {
        const targetFront = await processDescriptor.getTarget();

        // Don't create a proxy for the "main" target,
        // as we already created it in this.proxy.
        if (targetFront === this.hud.target) {
          continue;
        }

        if (!targetFront) {
          console.warn(
            "Can't retrieve the target front for process",
            processDescriptor
          );
          continue;
        }

        this.additionalProxies.push(
          new WebConsoleConnectionProxy(this, targetFront)
        );
      }
    }

    return Promise.all(this.getAllProxies().map(proxy => proxy.connect()));
  }

  _initUI() {
    this.document = this.window.document;
    this.rootElement = this.document.documentElement;

    this.outputNode = this.document.getElementById("app-wrapper");

    const toolbox = this.hud.toolbox;

    // Initialize module loader and load all the WebConsoleWrapper. The entire code-base
    // doesn't need any extra privileges and runs entirely in content scope.
    const WebConsoleWrapper = BrowserLoader({
      baseURI: "resource://devtools/client/webconsole/",
      window: this.window,
    }).require("./webconsole-wrapper");

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
        this.window.top.close.bind(this.window.top)
      );

      ZoomKeys.register(this.window, shortcuts);
      shortcuts.on("CmdOrCtrl+Alt+R", quickRestart);
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
   * Release an actor.
   *
   * @private
   * @param string actor
   *        The actor ID you want to release.
   */
  releaseActor(actor) {
    const proxy = this.getProxy();
    if (!proxy) {
      return null;
    }

    return proxy.releaseActor(actor);
  }

  /**
   * @param {String} expression
   * @param {Object} options
   * @returns {Promise}
   */
  evaluateJSAsync(expression, options) {
    return this.getProxy().webConsoleClient.evaluateJSAsync(
      expression,
      options
    );
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

  /**
   * Handler for the tabNavigated notification.
   *
   * @param string event
   *        Event name.
   * @param object packet
   *        Notification packet received from the server.
   */
  async handleTabNavigated(packet) {
    if (!packet.nativeConsoleAPI) {
      this.logWarningAboutReplacedAPI();
    }

    // Wait for completion of any async dispatch before notifying that the console
    // is fully updated after a page reload
    await this.wrapper.waitAsyncDispatches();
    this.emit("reloaded");
  }

  handleTabWillNavigate(packet) {
    this.wrapper.dispatchTabWillNavigate(packet);
  }
}

/* This is the same as DevelopmentHelpers.quickRestart, but it runs in all
 * builds (even official). This allows a user to do a restart + session restore
 * with Ctrl+Shift+J (open Browser Console) and then Ctrl+Shift+R (restart).
 */
function quickRestart() {
  const { Cc, Ci } = require("chrome");
  Services.obs.notifyObservers(null, "startupcache-invalidate");
  const env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  env.set("MOZ_DISABLE_SAFE_MODE_KEY", "1");
  Services.startup.quit(
    Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart
  );
}

exports.WebConsoleUI = WebConsoleUI;
