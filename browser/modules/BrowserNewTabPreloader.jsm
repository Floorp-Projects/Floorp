/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let EXPORTED_SYMBOLS = ["BrowserNewTabPreloader"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

const PREF_NEWTAB_URL = "browser.newtab.url";
const PREF_NEWTAB_PRELOAD = "browser.newtab.preload";

function BrowserNewTabPreloader() {
}

BrowserNewTabPreloader.prototype = {
  _url: null,
  _window: null,
  _browser: null,
  _enabled: null,

  init: function Preloader_init(aWindow) {
    if (this._window) {
      return;
    }

    this._window = aWindow;
    this._enabled = Preferences.enabled;
    this._url = Preferences.url;
    Preferences.addObserver(this);

    if (this._enabled) {
      this._createBrowser();
    }
  },

  uninit: function Preloader_uninit() {
    if (!this._window) {
      return;
    }

    if (this._browser) {
      this._browser.parentNode.removeChild(this._browser);
      this._browser = null;
    }

    this._window = null;
    Preferences.removeObserver(this);
  },

  newTab: function Preloader_newTab(aTab) {
    if (!this._window || !this._enabled) {
      return;
    }

    let tabbrowser = this._window.gBrowser;
    if (tabbrowser && this._isPreloaded()) {
      tabbrowser.swapNewTabWithBrowser(aTab, this._browser);
    }
  },

  observe: function Preloader_observe(aEnabled, aURL) {
    if (this._url != aURL) {
      this._url = aURL;

      if (this._enabled && aEnabled) {
        // We're still enabled but the newtab URL has changed.
        this._browser.setAttribute("src", aURL);
        return;
      }
    }

    if (this._enabled && !aEnabled) {
      // We got disabled. Remove the browser.
      this._browser.parentNode.removeChild(this._browser);
      this._browser = null;
      this._enabled = false;
    } else if (!this._enabled && aEnabled) {
      // We got enabled. Create a browser and start preloading.
      this._createBrowser();
      this._enabled = true;
    }
  },

  _createBrowser: function Preloader_createBrowser() {
    let document = this._window.document;
    this._browser = document.createElement("browser");
    this._browser.setAttribute("type", "content");
    this._browser.setAttribute("src", this._url);
    this._browser.collapsed = true;

    let panel = document.getElementById("browser-panel");
    panel.appendChild(this._browser);
  },

  _isPreloaded: function Preloader_isPreloaded()  {
    return this._browser &&
           this._browser.contentDocument &&
           this._browser.contentDocument.readyState == "complete" &&
           this._browser.currentURI.spec == this._url;
  }
};

let Preferences = {
  _observers: [],

  get _branch() {
    delete this._branch;
    return this._branch = Services.prefs.getBranch("browser.newtab.");
  },

  get enabled() {
    if (!this._branch.getBoolPref("preload")) {
      return false;
    }

    if (this._branch.prefHasUserValue("url")) {
      return false;
    }

    let url = this.url;
    return url && url != "about:blank";
  },

  get url() {
    return this._branch.getCharPref("url");
  },

  addObserver: function Preferences_addObserver(aObserver) {
    let index = this._observers.indexOf(aObserver);
    if (index == -1) {
      if (this._observers.length == 0) {
        this._branch.addObserver("", this, false);
      }
      this._observers.push(aObserver);
    }
  },

  removeObserver: function Preferences_removeObserver(aObserver) {
    let index = this._observers.indexOf(aObserver);
    if (index > -1) {
      if (this._observers.length == 1) {
        this._branch.removeObserver("", this);
      }
      this._observers.splice(index, 1);
    }
  },

  observe: function Preferences_observe(aSubject, aTopic, aData) {
    let url = this.url;
    let enabled = this.enabled;

    for (let obs of this._observers) {
      obs.observe(enabled, url);
    }
  }
};
