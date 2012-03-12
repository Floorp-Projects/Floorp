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
 *   Mihai Sucan <mihai.sucan@gmail.com>
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
  this.breakpoints = {};
}

DebuggerPane.prototype = {
  /**
   * Skip editor breakpoint change events.
   *
   * This property tells the source editor event handler to skip handling of
   * the BREAKPOINT_CHANGE events. This is used when the debugger adds/removes
   * breakpoints from the editor. Typically, the BREAKPOINT_CHANGE event handler
   * adds/removes events from the debugger, but when breakpoints are added from
   * the public debugger API, we need to do things in reverse.
   *
   * This implementation relies on the fact that the source editor fires the
   * BREAKPOINT_CHANGE events synchronously.
   *
   * @private
   * @type boolean
   */
  _skipEditorBreakpointChange: false,

  /**
   * The list of breakpoints in the debugger as tracked by the current
   * DebuggerPane instance. This an object where the values are BreakpointActor
   * objects received from the client, while the keys are actor names, for
   * example "conn0.breakpoint3".
   *
   * @type object
   */
  breakpoints: null,

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
      self.frame.contentWindow.updateEditorBreakpoints =
        self._updateEditorBreakpoints.bind(self);

      let config = {
        mode: SourceEditor.MODES.JAVASCRIPT,
        showLineNumbers: true,
        readOnly: true,
        showAnnotationRuler: true,
        showOverviewRuler: true,
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
    this.editor.addEventListener(SourceEditor.EVENTS.BREAKPOINT_CHANGE,
                                 this._onEditorBreakpointChange.bind(this));
    // Connect to the debugger server.
    this.connect();
  },

  /**
   * Event handler for breakpoint changes that happen in the editor. This
   * function syncs the breakpoint changes in the editor to those in the
   * debugger.
   *
   * @private
   * @param object aEvent
   *        The SourceEditor.EVENTS.BREAKPOINT_CHANGE event object.
   */
  _onEditorBreakpointChange: function DP__onEditorBreakpointChange(aEvent) {
    if (this._skipEditorBreakpointChange) {
      return;
    }

    aEvent.added.forEach(this._onEditorBreakpointAdd, this);
    aEvent.removed.forEach(this._onEditorBreakpointRemove, this);
  },

  /**
   * Retrieve the URL of the selected script in the debugger view.
   *
   * @private
   * @return string
   *         The URL of the selected script.
   */
  _selectedScript: function DP__selectedScript() {
    return this.debuggerWindow ?
           this.debuggerWindow.DebuggerView.Scripts.selected : null;
  },

  /**
   * Event handler for new breakpoints that come from the editor.
   *
   * @private
   * @param object aBreakpoint
   *        The breakpoint object coming from the editor.
   */
  _onEditorBreakpointAdd: function DP__onEditorBreakpointAdd(aBreakpoint) {
    let location = {
      url: this._selectedScript(),
      line: aBreakpoint.line + 1,
    };

    if (location.url) {
      let callback = function (aClient, aError) {
        if (aError) {
          this._skipEditorBreakpointChange = true;
          let result = this.editor.removeBreakpoint(aBreakpoint.line);
          this._skipEditorBreakpointChange = false;
        }
      }.bind(this);
      this.addBreakpoint(location, callback, true);
    }
  },

  /**
   * Event handler for breakpoints that are removed from the editor.
   *
   * @private
   * @param object aBreakpoint
   *        The breakpoint object that was removed from the editor.
   */
  _onEditorBreakpointRemove: function DP__onEditorBreakpointRemove(aBreakpoint) {
    let url = this._selectedScript();
    let line = aBreakpoint.line + 1;
    if (!url) {
      return;
    }

    let breakpoint = this.getBreakpoint(url, line);
    if (breakpoint) {
      this.removeBreakpoint(breakpoint, null, true);
    }
  },

  /**
   * Update the breakpoints in the editor view. This function takes the list of
   * breakpoints in the debugger and adds them back into the editor view. This
   * is invoked when the selected script is changed.
   *
   * @private
   */
  _updateEditorBreakpoints: function DP__updateEditorBreakpoints()
  {
    let url = this._selectedScript();
    if (!url) {
      return;
    }

    this._skipEditorBreakpointChange = true;
    for each (let breakpoint in this.breakpoints) {
      if (breakpoint.location.url == url) {
        this.editor.addBreakpoint(breakpoint.location.line - 1);
      }
    }
    this._skipEditorBreakpointChange = false;
  },

  /**
   * Add a breakpoint.
   *
   * @param object aLocation
   *        The location where you want the breakpoint. This object must have
   *        two properties:
   *          - url - the URL of the script.
   *          - line - the line number (starting from 1).
   * @param function [aCallback]
   *        Optional function to invoke once the breakpoint is added. The
   *        callback is invoked with two arguments:
   *          - aBreakpointClient - the BreakpointActor client object, if the
   *          breakpoint has been added successfully.
   *          - aResponseError - if there was any error.
   * @param boolean [aNoEditorUpdate=false]
   *        Tells if you want to skip editor updates. Typically the editor is
   *        updated to visually indicate that a breakpoint has been added.
   */
  addBreakpoint:
  function DP_addBreakpoint(aLocation, aCallback, aNoEditorUpdate) {
    let breakpoint = this.getBreakpoint(aLocation.url, aLocation.line);
    if (breakpoint) {
      aCallback && aCallback(breakpoint);
      return;
    }

    this.activeThread.setBreakpoint(aLocation, function(aResponse, aBpClient) {
      if (!aResponse.error) {
        this.breakpoints[aBpClient.actor] = aBpClient;

        if (!aNoEditorUpdate) {
          let url = this._selectedScript();
          if (url == aLocation.url) {
            this._skipEditorBreakpointChange = true;
            this.editor.addBreakpoint(aLocation.line - 1);
            this._skipEditorBreakpointChange = false;
          }
        }
      }

      aCallback && aCallback(aBpClient, aResponse.error);
    }.bind(this));
  },

  /**
   * Remove a breakpoint.
   *
   * @param object aBreakpoint
   *        The breakpoint you want to remove.
   * @param function [aCallback]
   *        Optional function to invoke once the breakpoint is removed. The
   *        callback is invoked with one argument: the breakpoint location
   *        object which holds the url and line properties.
   * @param boolean [aNoEditorUpdate=false]
   *        Tells if you want to skip editor updates. Typically the editor is
   *        updated to visually indicate that a breakpoint has been removed.
   */
  removeBreakpoint:
  function DP_removeBreakpoint(aBreakpoint, aCallback, aNoEditorUpdate) {
    if (!(aBreakpoint.actor in this.breakpoints)) {
      aCallback && aCallback(aBreakpoint.location);
      return;
    }

    aBreakpoint.remove(function() {
      delete this.breakpoints[aBreakpoint.actor];

      if (!aNoEditorUpdate) {
        let url = this._selectedScript();
        if (url == aBreakpoint.location.url) {
          this._skipEditorBreakpointChange = true;
          this.editor.removeBreakpoint(aBreakpoint.location.line - 1);
          this._skipEditorBreakpointChange = false;
        }
      }

      aCallback && aCallback(aBreakpoint.location);
    }.bind(this));
  },

  /**
   * Get the breakpoint object at the given location.
   *
   * @param string aUrl
   *        The URL of where the breakpoint is.
   * @param number aLine
   *        The line number where the breakpoint is.
   * @return object
   *         The BreakpointActor object.
   */
  getBreakpoint: function DP_getBreakpoint(aUrl, aLine) {
    for each (let breakpoint in this.breakpoints) {
      if (breakpoint.location.url == aUrl && breakpoint.location.line == aLine) {
        return breakpoint;
      }
    }
    return null;
  },

  /**
   * Closes the debugger UI removing child nodes and event listeners.
   */
  close: function DP_close() {
    for each (let breakpoint in this.breakpoints) {
      this.removeBreakpoint(breakpoint);
    }

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
    return this.frame ? this.frame.contentWindow : null;
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
    dbg.editor.resetUndo();
    let doc = dbg.frame.contentDocument;
    let scripts = doc.getElementById("scripts");
    let elt = scripts.getElementsByAttribute("value", aSourceUrl)[0];
    let script = elt.getUserData("sourceScript");
    script.loaded = true;
    script.text = aSourceText;
    script.contentType = aContentType;
    elt.setUserData("sourceScript", script, null);
    dbg._updateEditorBreakpoints();
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
