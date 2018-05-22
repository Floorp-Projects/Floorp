/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Utils: WebConsoleUtils} = require("devtools/client/webconsole/utils");
const EventEmitter = require("devtools/shared/event-emitter");
const promise = require("promise");
const defer = require("devtools/shared/defer");
const Services = require("Services");
const { gDevTools } = require("devtools/client/framework/devtools");
const { WebConsoleConnectionProxy } = require("devtools/client/webconsole/webconsole-connection-proxy");
const KeyShortcuts = require("devtools/client/shared/key-shortcuts");
const { l10n } = require("devtools/client/webconsole/utils/messages");

loader.lazyRequireGetter(this, "AppConstants", "resource://gre/modules/AppConstants.jsm", true);

const ZoomKeys = require("devtools/client/shared/zoom-keys");

const PREF_MESSAGE_TIMESTAMP = "devtools.webconsole.timestampMessages";
const PREF_PERSISTLOG = "devtools.webconsole.persistlog";
const PREF_SIDEBAR_ENABLED = "devtools.webconsole.sidebarToggle";

// XXX: This file is incomplete (see bug 1326937).
// It's used when loading the webconsole with devtools-launchpad, but will ultimately be
// the entry point for the new frontend

/**
 * A WebConsoleFrame instance is an interactive console initialized *per target*
 * that displays console log data as well as provides an interactive terminal to
 * manipulate the target's document content.
 *
 * The WebConsoleFrame is responsible for the actual Web Console UI
 * implementation.
 *
 * @constructor
 * @param object webConsoleOwner
 *        The WebConsole owner object.
 */
function NewWebConsoleFrame(webConsoleOwner) {
  this.owner = webConsoleOwner;
  this.hudId = this.owner.hudId;
  this.isBrowserConsole = this.owner._browserConsole;
  this.window = this.owner.iframeWindow;

  this._onToolboxPrefChanged = this._onToolboxPrefChanged.bind(this);
  this._onPanelSelected = this._onPanelSelected.bind(this);
  this._onChangeSplitConsoleState = this._onChangeSplitConsoleState.bind(this);

  EventEmitter.decorate(this);
}
NewWebConsoleFrame.prototype = {
  /**
   * Getter for the debugger WebConsoleClient.
   * @type object
   */
  get webConsoleClient() {
    return this.proxy ? this.proxy.webConsoleClient : null;
  },

  /**
   * Getter for the persistent logging preference.
   * @type boolean
   */
  get persistLog() {
    // For the browser console, we receive tab navigation
    // when the original top level window we attached to is closed,
    // but we don't want to reset console history and just switch to
    // the next available window.
    return this.isBrowserConsole ||
           Services.prefs.getBoolPref(PREF_PERSISTLOG);
  },

  /**
   * Initialize the WebConsoleFrame instance.
   * @return object
   *         A promise object that resolves once the frame is ready to use.
   */
  async init() {
    this._initUI();
    await this._initConnection();
    await this.newConsoleOutput.init();

    let id = WebConsoleUtils.supportsString(this.hudId);
    if (Services.obs) {
      Services.obs.notifyObservers(id, "web-console-created");
    }
  },
  destroy() {
    if (this._destroyer) {
      return this._destroyer.promise;
    }
    this._destroyer = defer();
    Services.prefs.removeObserver(PREF_MESSAGE_TIMESTAMP, this._onToolboxPrefChanged);
    this.React = this.ReactDOM = this.FrameView = null;
    if (this.jsterm) {
      this.jsterm.destroy();
      this.jsterm = null;
    }

    let toolbox = gDevTools.getToolbox(this.owner.target);
    if (toolbox) {
      toolbox.off("webconsole-selected", this._onPanelSelected);
      toolbox.off("split-console", this._onChangeSplitConsoleState);
      toolbox.off("select", this._onChangeSplitConsoleState);
    }

    this.window = this.owner = this.newConsoleOutput = null;

    let onDestroy = () => {
      this._destroyer.resolve(null);
    };
    if (this.proxy) {
      this.proxy.disconnect().then(onDestroy);
      this.proxy = null;
    } else {
      onDestroy();
    }

    return this._destroyer.promise;
  },

  _onUpdateListeners() {

  },

  logWarningAboutReplacedAPI() {
    this.owner.target.logWarningInPage(l10n.getStr("ConsoleAPIDisabled"),
      "ConsoleAPIDisabled");
  },

  handleNetworkEventUpdate() {

  },

  /**
   * Setter for saving of network request and response bodies.
   *
   * @param boolean value
   *        The new value you want to set.
   */
  setSaveRequestAndResponseBodies(value) {
    if (!this.webConsoleClient) {
      // Don't continue if the webconsole disconnected.
      return promise.resolve(null);
    }

    let deferred = defer();
    let newValue = !!value;
    let toSet = {
      "NetworkMonitor.saveRequestAndResponseBodies": newValue,
    };

    // Make sure the web console client connection is established first.
    this.webConsoleClient.setPreferences(toSet, response => {
      if (!response.error) {
        this._saveRequestAndResponseBodies = newValue;
        deferred.resolve(response);
      } else {
        deferred.reject(response.error);
      }
    });

    return deferred.promise;
  },

  /**
   * Connect to the server using the remote debugging protocol.
   *
   * @private
   * @return object
   *         A promise object that is resolved/reject based on the connection
   *         result.
   */
  _initConnection: function() {
    if (this._initDefer) {
      return this._initDefer.promise;
    }

    this._initDefer = defer();
    this.proxy = new WebConsoleConnectionProxy(this, this.owner.target);

    this.proxy.connect().then(() => {
      // on success
      this._initDefer.resolve(this);
    }, (reason) => {
      // on failure
      // TODO Print a message to console
      this._initDefer.reject(reason);
    });

    return this._initDefer.promise;
  },

  _initUI: function() {
    this.document = this.window.document;
    this.rootElement = this.document.documentElement;

    this.outputNode = this.document.getElementById("output-container");

    let toolbox = gDevTools.getToolbox(this.owner.target);

    // Handle both launchpad and toolbox loading
    let Wrapper = this.owner.NewConsoleOutputWrapper || this.window.NewConsoleOutput;
    this.newConsoleOutput =
      new Wrapper(this.outputNode, this, toolbox, this.owner, this.document);
    // Toggle the timestamp on preference change
    Services.prefs.addObserver(PREF_MESSAGE_TIMESTAMP, this._onToolboxPrefChanged);
    this._onToolboxPrefChanged();

    this._initShortcuts();

    if (toolbox) {
      toolbox.on("webconsole-selected", this._onPanelSelected);
      toolbox.on("split-console", this._onChangeSplitConsoleState);
      toolbox.on("select", this._onChangeSplitConsoleState);
    }
  },

  _initShortcuts: function() {
    let shortcuts = new KeyShortcuts({
      window: this.window
    });

    shortcuts.on(l10n.getStr("webconsole.find.key"),
                 event => {
                   this.filterBox.focus();
                   event.preventDefault();
                 });

    let clearShortcut;
    if (AppConstants.platform === "macosx") {
      clearShortcut = l10n.getStr("webconsole.clear.keyOSX");
    } else {
      clearShortcut = l10n.getStr("webconsole.clear.key");
    }

    shortcuts.on(clearShortcut, () => this.jsterm.clearOutput(true));

    if (this.isBrowserConsole) {
      // Make sure keyboard shortcuts work immediately after opening
      // the Browser Console (Bug 1461366).
      this.window.focus();

      shortcuts.on(l10n.getStr("webconsole.close.key"),
                   this.window.top.close.bind(this.window.top));

      ZoomKeys.register(this.window);
      shortcuts.on("CmdOrCtrl+Alt+R", quickRestart);
    } else if (Services.prefs.getBoolPref(PREF_SIDEBAR_ENABLED)) {
      shortcuts.on("Esc", event => {
        if (!this.jsterm.autocompletePopup || !this.jsterm.autocompletePopup.isOpen) {
          this.newConsoleOutput.dispatchSidebarClose();
          this.jsterm.focus();
        }
      });
    }
  },

  /**
   * Handler for page location changes.
   *
   * @param string uri
   *        New page location.
   * @param string title
   *        New page title.
   */
  onLocationChange: function(uri, title) {
    this.contentLocation = uri;
    if (this.owner.onLocationChange) {
      this.owner.onLocationChange(uri, title);
    }
  },

  /**
   * Release an actor.
   *
   * @private
   * @param string actor
   *        The actor ID you want to release.
   */
  _releaseObject: function(actor) {
    if (this.proxy) {
      this.proxy.releaseActor(actor);
    }
  },

  /**
   * Called when the message timestamp pref changes.
   */
  _onToolboxPrefChanged: function() {
    let newValue = Services.prefs.getBoolPref(PREF_MESSAGE_TIMESTAMP);
    this.newConsoleOutput.dispatchTimestampsToggle(newValue);
  },

  /**
   * Sets the focus to JavaScript input field when the web console tab is
   * selected or when there is a split console present.
   * @private
   */
  _onPanelSelected: function() {
    this.jsterm.focus();
  },

  _onChangeSplitConsoleState: function() {
    this.newConsoleOutput.dispatchSplitConsoleCloseButtonToggle();
  },

  /**
   * Handler for the tabNavigated notification.
   *
   * @param string event
   *        Event name.
   * @param object packet
   *        Notification packet received from the server.
   */
  handleTabNavigated: async function(packet) {
    if (packet.url) {
      this.onLocationChange(packet.url, packet.title);
    }

    if (!packet.nativeConsoleAPI) {
      this.logWarningAboutReplacedAPI();
    }

    // Wait for completion of any async dispatch before notifying that the console
    // is fully updated after a page reload
    await this.newConsoleOutput.waitAsyncDispatches();
    this.emit("reloaded");
  },

  handleTabWillNavigate: function(packet) {
    if (this.persistLog) {
      // Add a _type to hit convertCachedPacket.
      packet._type = true;
      this.newConsoleOutput.dispatchMessageAdd(packet);
    } else {
      this.jsterm.clearOutput(false);
    }

    if (packet.url) {
      this.onLocationChange(packet.url, packet.title);
    }
  }
};

/* This is the same as DevelopmentHelpers.quickRestart, but it runs in all
 * builds (even official). This allows a user to do a restart + session restore
 * with Ctrl+Shift+J (open Browser Console) and then Ctrl+Shift+R (restart).
 */
function quickRestart() {
  const { Cc, Ci } = require("chrome");
  Services.obs.notifyObservers(null, "startupcache-invalidate");
  let env = Cc["@mozilla.org/process/environment;1"]
            .getService(Ci.nsIEnvironment);
  env.set("MOZ_DISABLE_SAFE_MODE_KEY", "1");
  Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart);
}

exports.NewWebConsoleFrame = NewWebConsoleFrame;
