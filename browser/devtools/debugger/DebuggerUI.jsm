/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Debugger UI code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com> (original author)
 *   Panos Astithas <past@mozilla.com>
 *   Victor Porof <vporof@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/devtools/dbg-server.jsm");
Cu.import("resource://gre/modules/devtools/dbg-client.jsm");
Cu.import("resource:///modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/source-editor.jsm");

let EXPORTED_SYMBOLS = ["DebuggerUI"];

/**
 * Creates a pane that will host the debugger UI.
 */
function DebuggerPane(aTab) {
  this._tab = aTab;
  this._close = this.close.bind(this);
  this._debugTab = this.debugTab.bind(this);
}

DebuggerPane.prototype = {
  /**
   * Creates and initializes the widgets contained in the debugger UI.
   */
  create: function DP_create(gBrowser) {
    this._tab._scriptDebugger = this;

    this._nbox = gBrowser.getNotificationBox(this._tab.linkedBrowser);
    this._splitter = gBrowser.parentNode.ownerDocument.createElement("splitter");
    this._splitter.setAttribute("class", "hud-splitter");
    this.frame = gBrowser.parentNode.ownerDocument.createElement("iframe");
    this.frame.height = DebuggerUIPreferences.height;

    this._nbox.appendChild(this._splitter);
    this._nbox.appendChild(this.frame);

    let self = this;

    this.frame.addEventListener("DOMContentLoaded", function initPane(aEvent) {
      if (aEvent.target != self.frame.contentDocument) {
        return;
      }
      self.frame.removeEventListener("DOMContentLoaded", initPane, true);
      // Initialize the source editor.
      self.frame.contentWindow.editor = self.editor = new SourceEditor();

      let config = {
        mode: SourceEditor.MODES.JAVASCRIPT,
        showLineNumbers: true,
        readOnly: true
      };

      let editorPlaceholder = self.frame.contentDocument.getElementById("editor");
      self.editor.init(editorPlaceholder, config, self._onEditorLoad.bind(self));
    }, true);
    this.frame.addEventListener("DebuggerClose", this._close, true);

    this.frame.setAttribute("src", "chrome://browser/content/debugger.xul");
  },

  /**
   * The load event handler for the source editor. This method does post-load
   * editor initialization.
   */
  _onEditorLoad: function DP__onEditorLoad() {
    // Connect to the debugger server.
    this.connect();
  },

  /**
   * Closes the debugger UI removing child nodes and event listeners.
   */
  close: function DP_close() {
    if (this._tab) {
      this._tab._scriptDebugger = null;
      this._tab = null;
    }
    if (this.frame) {
      DebuggerUIPreferences.height = this.frame.height;

      this.frame.removeEventListener("unload", this._close, true);
      this.frame.removeEventListener("DebuggerClose", this._close, true);
      if (this.frame.parentNode) {
        this.frame.parentNode.removeChild(this.frame);
      }
      this.frame = null;
    }
    if (this._nbox) {
      this._nbox.removeChild(this._splitter);
      this._nbox = null;
    }

    this._splitter = null;

    if (this._client) {
      this._client.removeListener("newScript", this.onNewScript);
      this._client.removeListener("tabDetached", this._close);
      this._client.removeListener("tabNavigated", this._debugTab);
      this._client.close();
      this._client = null;
    }
  },

  /**
   * Initializes a debugger client and connects it to the debugger server,
   * wiring event handlers as necessary.
   */
  connect: function DP_connect() {
    this.frame.addEventListener("unload", this._close, true);

    let transport = DebuggerServer.connectPipe();
    this._client = new DebuggerClient(transport);
    // Store the new script handler locally, so when it's time to remove it we
    // don't need to go through the iframe, since it might be cleared.
    this.onNewScript = this.debuggerWindow.SourceScripts.onNewScript;
    let self = this;
    this._client.addListener("tabNavigated", this._debugTab);
    this._client.addListener("tabDetached", this._close);
    this._client.addListener("newScript", this.onNewScript);
    this._client.connect(function(aType, aTraits) {
      self._client.listTabs(function(aResponse) {
        let tab = aResponse.tabs[aResponse.selected];
        self.debuggerWindow.startDebuggingTab(self._client, tab);
        if (self.onConnected) {
          self.onConnected(self);
        }
      });
    });
  },

  /**
   * Starts debugging the current tab. This function is called on each location
   * change in this tab.
   */
  debugTab: function DP_debugTab(aNotification, aPacket) {
    let self = this;
    this._client.activeThread.detach(function() {
      self._client.activeTab.detach(function() {
        self._client.listTabs(function(aResponse) {
          let tab = aResponse.tabs[aResponse.selected];
          self.debuggerWindow.startDebuggingTab(self._client, tab);
          if (self.onConnected) {
            self.onConnected(self);
          }
        });
      });
    });
  },

  get debuggerWindow() {
    return this.frame.contentWindow;
  },

  get debuggerClient() {
    return this._client;
  },

  get activeThread() {
    try {
      return this.debuggerWindow.ThreadState.activeThread;
    } catch(ex) {
      return undefined;
    }
  }
};

function DebuggerUI(aWindow) {
  this.aWindow = aWindow;

  aWindow.addEventListener("Debugger:LoadSource", this._onLoadSource.bind(this));
}

DebuggerUI.prototype = {
  /**
   * Starts the debugger or stops it, if it is already started.
   */
  toggleDebugger: function DebuggerUI_toggleDebugger() {
    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
      DebuggerServer.addBrowserActors();
    }

    let gBrowser = this.aWindow.gBrowser;
    let tab = gBrowser.selectedTab;

    if (tab._scriptDebugger) {
      // If the debugger is already open, just close it.
      tab._scriptDebugger.close();
      return tab._scriptDebugger;
    }

    let pane = new DebuggerPane(tab);
    pane.create(gBrowser);
    return pane;
  },

  getDebugger: function DebuggerUI_getDebugger(aTab) {
    return aTab._scriptDebugger;
  },

  get preferences() {
    return DebuggerUIPreferences;
  },

  /**
   * Handles notifications to load a source script from the cache or from a
   * local file.
   * XXX: it may be better to use nsITraceableChannel to get to the sources
   * without relying on caching when we can (not for eval, etc.):
   * http://www.softwareishard.com/blog/firebug/nsitraceablechannel-intercept-http-traffic/
   */
  _onLoadSource: function DebuggerUI__onLoadSource(aEvent) {
    let gBrowser = this.aWindow.gBrowser;

    let url = aEvent.detail;
    let scheme = Services.io.extractScheme(url);
    switch (scheme) {
      case "file":
      case "chrome":
      case "resource":
        try {
          NetUtil.asyncFetch(url, function onFetch(aStream, aStatus) {
            if (!Components.isSuccessCode(aStatus)) {
              return this.logError(url, aStatus);
            }
            let source = NetUtil.readInputStreamToString(aStream, aStream.available());
            aStream.close();
            this._onSourceLoaded(url, source);
          }.bind(this));
        } catch (ex) {
          return this.logError(url, ex.name);
        }
        break;

      default:
        let channel = Services.io.newChannel(url, null, null);
        let chunks = [];
        let streamListener = { // nsIStreamListener inherits nsIRequestObserver
          onStartRequest: function (aRequest, aContext, aStatusCode) {
            if (!Components.isSuccessCode(aStatusCode)) {
              return this.logError(url, aStatusCode);
            }
          }.bind(this),
          onDataAvailable: function (aRequest, aContext, aStream, aOffset, aCount) {
            chunks.push(NetUtil.readInputStreamToString(aStream, aCount));
          },
          onStopRequest: function (aRequest, aContext, aStatusCode) {
            if (!Components.isSuccessCode(aStatusCode)) {
              return this.logError(url, aStatusCode);
            }

            this._onSourceLoaded(url, chunks.join(""), channel.contentType);
          }.bind(this)
        };

        channel.loadFlags = channel.LOAD_FROM_CACHE;
        channel.asyncOpen(streamListener, null);
        break;
    }
  },

  /**
   * Log an error message in the error console when a script fails to load.
   *
   * @param string aUrl
   *        The URL of the source script.
   * @param string aStatus
   *        The failure status code.
   */
  logError: function DebuggerUI_logError(aUrl, aStatus) {
    let view = this.getDebugger(gBrowser.selectedTab).DebuggerView;
    Components.utils.reportError(view.getFormatStr("loadingError", [ aUrl, aStatus ]));
  },

  /**
   * Called when source has been loaded.
   *
   * @param string aSourceUrl
   *        The URL of the source script.
   * @param string aSourceText
   *        The text of the source script.
   * @param string aContentType
   *        The content type of the source script.
   */
  _onSourceLoaded: function DebuggerUI__onSourceLoaded(aSourceUrl,
                                                       aSourceText,
                                                       aContentType) {
    let dbg = this.getDebugger(this.aWindow.gBrowser.selectedTab);
    dbg.debuggerWindow.SourceScripts.setEditorMode(aSourceUrl, aContentType);
    dbg.editor.setText(aSourceText);
    let doc = dbg.frame.contentDocument;
    let scripts = doc.getElementById("scripts");
    let elt = scripts.getElementsByAttribute("value", aSourceUrl)[0];
    let script = elt.getUserData("sourceScript");
    script.loaded = true;
    script.text = aSourceText;
    script.contentType = aContentType;
    elt.setUserData("sourceScript", script, null);
  }
};

/**
 * Various debugger UI preferences (currently just the pane height).
 */
let DebuggerUIPreferences = {

  _height: -1,

  /**
   * Gets the preferred height of the debugger pane.
   *
   * @return number
   *         The preferred height.
   */
  get height() {
    if (this._height < 0) {
      this._height = Services.prefs.getIntPref("devtools.debugger.ui.height");
    }
    return this._height;
  },

  /**
   * Sets the preferred height of the debugger pane.
   *
   * @param number value
   *        The new height.
   */
  set height(value) {
    Services.prefs.setIntPref("devtools.debugger.ui.height", value);
    this._height = value;
  }
};
