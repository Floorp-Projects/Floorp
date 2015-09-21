/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {utils: Cu, interfaces: Ci} = Components;
Cu.import("resource:///modules/devtools/gDevTools.jsm");
const {require} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
const {Toolbox} = require("devtools/framework/toolbox");
const {ConnectionManager, Connection} = require("devtools/client/connection-manager");
const promise = require("devtools/toolkit/deprecated-sync-thenables");
const prefs = require("sdk/preferences/service");
const Services = require("Services");
const Strings = Services.strings.createBundle("chrome://browser/locale/devtools/app-manager.properties");

var UI = {
  _toolboxTabCursor: 0,
  _handledTargets: new Map(),

  connection: null,

  init: function() {
    this.onLoad = this.onLoad.bind(this);
    this.onUnload = this.onUnload.bind(this);
    this.onMessage = this.onMessage.bind(this);
    this.onConnected = this.onConnected.bind(this);
    this.onDisconnected = this.onDisconnected.bind(this);

    window.addEventListener("load", this.onLoad);
    window.addEventListener("unload", this.onUnload);
    window.addEventListener("message", this.onMessage);
  },

  onLoad: function() {
    window.removeEventListener("load", this.onLoad);
    let defaultPanel = prefs.get("devtools.appmanager.lastTab");
    let panelExists = !!document.querySelector("." + defaultPanel  + "-panel");
    this.selectTab(panelExists ? defaultPanel : "projects");
    this.showDeprecationNotice();
  },

  onUnload: function() {
    for (let [target, toolbox] of this._handledTargets) {
      toolbox.destroy();
    }

    window.removeEventListener("unload", this.onUnload);
    window.removeEventListener("message", this.onMessage);
    if (this.connection) {
      this.connection.off(Connection.Status.CONNECTED, this.onConnected);
      this.connection.off(Connection.Status.DISCONNECTED, this.onDisconnected);
    }
  },

  onMessage: function(event) {
    try {
      let json = JSON.parse(event.data);
      switch (json.name) {
        case "connection":
          let cid = +json.cid;
          for (let c of ConnectionManager.connections) {
            if (c.uid == cid) {
              this.onNewConnection(c);
              break;
            }
          }
          break;
        case "closeHelp":
          this.selectTab("projects");
          break;
        case "toolbox-raise":
          window.top.focus();
          this.selectTab(json.uid);
          break;
        case "toolbox-close":
          this.closeToolboxTab(json.uid);
          break;
        case "toolbox-title":
          // Not implemented
          break;
        default:
          Cu.reportError("Unknown message: " + json.name);
      }
    } catch(e) { Cu.reportError(e); }

    // Forward message
    let panels = document.querySelectorAll(".panel");
    for (let frame of panels) {
      frame.contentWindow.postMessage(event.data, "*");
    }
  },

  selectTabFromButton: function(button) {
    if (!button.hasAttribute("panel"))
      return;
    this.selectTab(button.getAttribute("panel"));
  },

  selectTab: function(panel) {
    let isToolboxTab = false;
    for (let type of ["button", "panel"]) {
      let oldSelection = document.querySelector("." + type + "[selected]");
      let newSelection = document.querySelector("." + panel + "-" + type);
      if (oldSelection) oldSelection.removeAttribute("selected");
      if (newSelection) {
        newSelection.setAttribute("selected", "true");
        if (newSelection.classList.contains("toolbox")) {
          isToolboxTab = true;
        }
      }
    }
    if (!isToolboxTab) {
      prefs.set("devtools.appmanager.lastTab", panel);
    }
  },

  onNewConnection: function(connection) {
    this.connection = connection;
    this.connection.on(Connection.Status.CONNECTED, this.onConnected);
    this.connection.on(Connection.Status.DISCONNECTED, this.onDisconnected);
  },

  onConnected: function() {
    document.querySelector("#content").classList.add("connected");
  },

  onDisconnected: function() {
    for (let [,toolbox] of this._handledTargets) {
      if (toolbox) {
        toolbox.destroy();
      }
    }
    this._handledTargets.clear();
    document.querySelector("#content").classList.remove("connected");
  },

  createToolboxTab: function(name, iconURL, uid) {
    let button = document.createElement("button");
    button.className = "button toolbox " + uid + "-button";
    button.setAttribute("panel", uid);
    button.textContent = name;
    button.setAttribute("style", "background-image: url(" + iconURL + ")");
    let toolboxTabs = document.querySelector("#toolbox-tabs");
    toolboxTabs.appendChild(button);
    let iframe = document.createElement("iframe");
    iframe.setAttribute("flex", "1");
    iframe.className = "panel toolbox " + uid + "-panel";
    let panels = document.querySelector("#tab-panels");
    panels.appendChild(iframe);
    this.selectTab(uid);
    return iframe;
  },

  closeToolboxTab: function(uid) {
    let buttonToDestroy = document.querySelector("." + uid + "-button");
    let panelToDestroy = document.querySelector("." + uid + "-panel");

    if (buttonToDestroy.hasAttribute("selected")) {
      let lastTab = prefs.get("devtools.appmanager.lastTab");
      this.selectTab(lastTab);
    }

    buttonToDestroy.remove();
    panelToDestroy.remove();
  },

  openAndShowToolboxForTarget: function(target, name, icon) {
    let host = Toolbox.HostType.CUSTOM;
    let toolbox = gDevTools.getToolbox(target);
    if (!toolbox) {
      let uid = "uid" + this._toolboxTabCursor++;
      let iframe = this.createToolboxTab(name, icon, uid);
      let options = { customIframe: iframe , uid: uid };
      this._handledTargets.set(target, null);
      return gDevTools.showToolbox(target, null, host, options).then(toolbox => {
        this._handledTargets.set(target, toolbox);
        toolbox.once("destroyed", () => {
          this._handledTargets.delete(target)
        });
      });
    } else {
      return gDevTools.showToolbox(target, null, host);
    }
  },

  showDeprecationNotice: function() {
    let message = Strings.GetStringFromName("index.deprecationNotice");

    let buttons = [
      {
        label: Strings.GetStringFromName("index.launchWebIDE"),
        callback: gDevToolsBrowser.openWebIDE
      },
      {
        label: Strings.GetStringFromName("index.readMoreAboutWebIDE"),
        callback: () => {
          window.open("https://developer.mozilla.org/docs/Tools/WebIDE");
        }
      }
    ];

    let docShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIWebNavigation)
                   .QueryInterface(Ci.nsIDocShell);
    let browser = docShell.chromeEventHandler;
    let nbox = browser.ownerDocument.defaultView.gBrowser
               .getNotificationBox(browser);
    nbox.appendNotification(message, "app-manager-deprecation", null,
                            nbox.PRIORITY_WARNING_LOW, buttons);
  }
};

UI.init();
