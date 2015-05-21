/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

this.EXPORTED_SYMBOLS = ["UserCustomizations"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

XPCOMUtils.defineLazyServiceGetter(this, "console",
                                   "@mozilla.org/consoleservice;1",
                                   "nsIConsoleService");
/**
  * Customization scripts and CSS stylesheets can be specified in an
  * application manifest with the following syntax:
  * "customizations": [
  *  {
  *    "filter": "http://youtube.com",
  *    "css": ["file1.css", "file2.css"],
  *    "scripts": ["script1.js", "script2.js"]
  *  }
  * ]
  */

function debug(aMsg) {
  if (!UserCustomizations._debug) {
    return;
  }
  dump("-*-*- UserCustomizations (" +
           (UserCustomizations._inParent ? "parent" : "child") +
           "): " + aMsg + "\n");
}

function log(aStr) {
  console.logStringMessage(aStr);
}

this.UserCustomizations = {
  _debug: false,
  _items: [],
  _loaded : {},   // Keep track per manifestURL of css and scripts loaded.
  _windows: null, // Set of currently opened windows.
  _enabled: false,

  _addItem: function(aItem) {
    debug("_addItem: " + uneval(aItem));
    this._items.push(aItem);
    if (this._inParent) {
      ppmm.broadcastAsyncMessage("UserCustomizations:Add", [aItem]);
    }
  },

  _removeItem: function(aHash) {
    debug("_removeItem: " + aHash);
    let index = -1;
    this._items.forEach((script, pos) => {
      if (script.hash == aHash ) {
        index = pos;
      }
    });

    if (index != -1) {
      this._items.splice(index, 1);
    }

    if (this._inParent) {
      ppmm.broadcastAsyncMessage("UserCustomizations:Remove", aHash);
    }
  },

  register: function(aManifest, aApp) {
    debug("Starting customization registration for " + aApp.manifestURL);

    if (!this._enabled || !aApp.enabled || aApp.role != "addon") {
      debug("Rejecting registration (global enabled=" + this._enabled +
            ") (app role=" + aApp.role +
            ", enabled=" + aApp.enabled + ")");
      debug(uneval(aApp));
      return;
    }

    let customizations = aManifest.customizations;
    if (customizations === undefined || !Array.isArray(customizations)) {
      return;
    }

    let base = Services.io.newURI(aApp.origin, null, null);

    customizations.forEach(item => {
      // The filter property is mandatory.
      if (!item.filter || (typeof item.filter !== "string")) {
        log("Mandatory filter property not found in this customization item: " +
            uneval(item) + " in " + aApp.manifestURL);
        return;
      }

      // Create a new object with resolved urls and a hash that we reuse to
      // remove items.
      let custom = {
        filter: item.filter,
        status: aApp.appStatus,
        manifestURL: aApp.manifestURL,
        css: [],
        scripts: []
      };
      custom.hash = AppsUtils.computeObjectHash(item);

      if (item.css && Array.isArray(item.css)) {
        item.css.forEach((css) => {
          custom.css.push(base.resolve(css));
        });
      }

      if (item.scripts && Array.isArray(item.scripts)) {
        item.scripts.forEach((script) => {
          custom.scripts.push(base.resolve(script));
        });
      }

      this._addItem(custom);
    });
    this._updateAllWindows();
  },

  _updateAllWindows: function() {
    debug("UpdateWindows");
    if (this._inParent) {
      ppmm.broadcastAsyncMessage("UserCustomizations:UpdateWindows", {});
    }
    // Inject in all currently opened windows.
    this._windows.forEach(this._injectInWindow.bind(this));
  },

  unregister: function(aManifest, aApp) {
    if (!this._enabled) {
      return;
    }

    debug("Starting customization unregistration for " + aApp.manifestURL);
    let customizations = aManifest.customizations;
    if (customizations === undefined || !Array.isArray(customizations)) {
      return;
    }

    customizations.forEach(item => {
      this._removeItem(AppsUtils.computeObjectHash(item));
    });
    this._unloadForManifestURL(aApp.manifestURL);
  },

  _unloadForManifestURL: function(aManifestURL) {
    debug("_unloadForManifestURL " + aManifestURL);

    if (this._inParent) {
      ppmm.broadcastAsyncMessage("UserCustomizations:Unload", aManifestURL);
    }

    if (!this._loaded[aManifestURL]) {
      return;
    }

    if (this._loaded[aManifestURL].scripts &&
        this._loaded[aManifestURL].scripts.length > 0) {
      // We can't rollback script changes, so don't even try to unload in this
      // situation.
      return;
    }

    this._loaded[aManifestURL].css.forEach(aItem => {
      try {
        debug("unloading " + aItem.uri.spec);
        let utils = aItem.window.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDOMWindowUtils);
        utils.removeSheet(aItem.uri, Ci.nsIDOMWindowUtils.AUTHOR_SHEET);
      } catch(e) {
        log("Error unloading stylesheet " + aItem.uri.spec + " : " + e);
      }
    });

    this._loaded[aManifestURL] = null;
  },

  _injectItem: function(aWindow, aItem, aInjected) {
    debug("Injecting item " + uneval(aItem) + " in " + aWindow.location.href);
    let utils = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils);

    let manifestURL = aItem.manifestURL;

    // Load the stylesheets only in this window.
    aItem.css.forEach(aCss => {
      if (aInjected.indexOf(aCss) !== -1) {
        debug("Skipping duplicated css: " + aCss);
        return;
      }

      let uri = Services.io.newURI(aCss, null, null);
      try {
        utils.loadSheet(uri, Ci.nsIDOMWindowUtils.AUTHOR_SHEET);
        if (!this._loaded[manifestURL]) {
          this._loaded[manifestURL] = { css: [], scripts: [] };
        }
        this._loaded[manifestURL].css.push({ window: aWindow, uri: uri });
        aInjected.push(aCss);
      } catch(e) {
        log("Error loading stylesheet " + aCss + " : " + e);
      }
    });

    let sandbox;
    if (aItem.scripts.length > 0) {
      sandbox = Cu.Sandbox([aWindow],
                           { wantComponents: false,
                             sandboxPrototype: aWindow });
    }

    // Load the scripts using a sandbox.
    aItem.scripts.forEach(aScript => {
      debug("Sandboxing " + aScript);
      if (aInjected.indexOf(aScript) !== -1) {
        debug("Skipping duplicated script: " + aScript);
        return;
      }

      try {
        let options = {
          target: sandbox,
          charset: "UTF-8",
          async: true
        }
        Services.scriptloader.loadSubScriptWithOptions(aScript, options);
        if (!this._loaded[manifestURL]) {
          this._loaded[manifestURL] = { css: [], scripts: [] };
        }
        this._loaded[manifestURL].scripts.push({ sandbox: sandbox, uri: aScript });
        aInjected.push(aScript);
      } catch(e) {
        log("Error sandboxing " + aScript + " : " + e);
      }
    });

    // Makes sure we get rid of the sandbox.
    if (sandbox) {
      aWindow.addEventListener("unload", () => {
        Cu.nukeSandbox(sandbox);
        sandbox = null;
      });
    }
  },

  _injectInWindow: function(aWindow) {
    debug("_injectInWindow");

    if (!aWindow || !aWindow.document) {
      return;
    }

    let principal = aWindow.document.nodePrincipal;
    debug("principal status: " + principal.appStatus);

    let href = aWindow.location.href;

    // The list of resources loaded in this window, used to filter out
    // duplicates.
    let injected = [];

    this._items.forEach((aItem) => {
      // We only allow customizations to apply to apps with an equal or lower
      // privilege level.
      if (principal.appStatus > aItem.status) {
        return;
      }

      let regexp = new RegExp(aItem.filter, "g");
      if (regexp.test(href)) {
        this._injectItem(aWindow, aItem, injected);
        debug("Currently injected: " + injected.toString());
      }
    });
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic === "content-document-global-created") {
      let window = aSubject.QueryInterface(Ci.nsIDOMWindow);
      let href = window.location.href;
      if (!href || href == "about:blank") {
        return;
      }

      let id = window.QueryInterface(Ci.nsIInterfaceRequestor)
                     .getInterface(Ci.nsIDOMWindowUtils)
                     .currentInnerWindowID;
      this._windows.set(id, window);

      debug("document created: " + href);
      this._injectInWindow(window);
    } else if (aTopic === "inner-window-destroyed") {
      let winId = aSubject.QueryInterface(Ci.nsISupportsPRUint64).data;
      this._windows.delete(winId);
    }
  },

  init: function() {
    this._enabled = false;
    try {
      this._enabled = Services.prefs.getBoolPref("dom.apps.customization.enabled");
    } catch(e) {}

    if (!this._enabled) {
      return;
    }

    this._windows = new Map(); // Can't be a WeakMap because we need to enumerate.
    this._inParent = Cc["@mozilla.org/xre/runtime;1"]
                       .getService(Ci.nsIXULRuntime)
                       .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

    debug("init");

    Services.obs.addObserver(this, "content-document-global-created",
                             /* ownsWeak */ false);
    Services.obs.addObserver(this, "inner-window-destroyed",
                             /* ownsWeak */ false);

    if (this._inParent) {
      ppmm.addMessageListener("UserCustomizations:List", this);
    } else {
      cpmm.addMessageListener("UserCustomizations:Add", this);
      cpmm.addMessageListener("UserCustomizations:Remove", this);
      cpmm.addMessageListener("UserCustomizations:Unload", this);
      cpmm.addMessageListener("UserCustomizations:UpdateWindows", this);
      cpmm.sendAsyncMessage("UserCustomizations:List", {});
    }
  },

  receiveMessage: function(aMessage) {
    let name = aMessage.name;
    let data = aMessage.data;

    switch(name) {
      case "UserCustomizations:List":
        aMessage.target.sendAsyncMessage("UserCustomizations:Add", this._items);
        break;
      case "UserCustomizations:Add":
        data.forEach(this._addItem, this);
        break;
      case "UserCustomizations:Remove":
        this._removeItem(data);
        break;
      case "UserCustomizations:Unload":
        this._unloadForManifestURL(data);
        break;
      case "UserCustomizations:UpdateWindows":
        this._updateAllWindows();
        break;
    }
  }
}

UserCustomizations.init();
