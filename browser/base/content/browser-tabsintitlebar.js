/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var TabsInTitlebar = {
  init() {
    this._readPref();
    Services.prefs.addObserver(this._prefName, this);

    gDragSpaceObserver.init();
    this._initialized = true;
    this._update();
  },

  allowedBy(condition, allow) {
    if (allow) {
      if (condition in this._disallowed) {
        delete this._disallowed[condition];
        this._update();
      }
    } else if (!(condition in this._disallowed)) {
      this._disallowed[condition] = null;
      this._update();
    }
  },

  get systemSupported() {
    let isSupported = false;
    switch (AppConstants.MOZ_WIDGET_TOOLKIT) {
      case "windows":
      case "cocoa":
        isSupported = true;
        break;
      case "gtk3":
        isSupported = window.matchMedia("(-moz-gtk-csd-available)");
        break;
    }
    delete this.systemSupported;
    return this.systemSupported = isSupported;
  },

  get enabled() {
    return document.documentElement.getAttribute("tabsintitlebar") == "true";
  },

  observe(subject, topic, data) {
    if (topic == "nsPref:changed")
      this._readPref();
  },

  _initialized: false,
  _disallowed: {},
  _prefName: "browser.tabs.drawInTitlebar",

  _readPref() {
    this.allowedBy("pref",
                   Services.prefs.getBoolPref(this._prefName));
  },

  _update() {
    if (!this._initialized) {
      return;
    }

    let allowed = this.systemSupported &&
                  !window.fullScreen &&
                  (Object.keys(this._disallowed)).length == 0;
    if (allowed) {
      document.documentElement.setAttribute("tabsintitlebar", "true");
      if (AppConstants.platform == "macosx") {
        document.documentElement.setAttribute("chromemargin", "0,-1,-1,-1");
        document.documentElement.removeAttribute("drawtitle");
      } else {
        document.documentElement.setAttribute("chromemargin", "0,2,2,2");
      }
    } else {
      document.documentElement.removeAttribute("tabsintitlebar");
      document.documentElement.removeAttribute("chromemargin");
      if (AppConstants.platform == "macosx") {
        document.documentElement.setAttribute("drawtitle", "true");
      }
    }

    ToolbarIconColor.inferFromText("tabsintitlebar", allowed);
  },

  uninit() {
    Services.prefs.removeObserver(this._prefName, this);
    gDragSpaceObserver.uninit();
  },
};

function onTitlebarMaxClick() {
  if (window.windowState == window.STATE_MAXIMIZED)
    window.restore();
  else
    window.maximize();
}

// Adds additional drag space to the window by listening to
// the corresponding preference.
var gDragSpaceObserver = {
  pref: "browser.tabs.extraDragSpace",

  init() {
    this._update();
    Services.prefs.addObserver(this.pref, this);
  },

  uninit() {
    Services.prefs.removeObserver(this.pref, this);
  },

  observe(aSubject, aTopic, aPrefName) {
    if (aTopic != "nsPref:changed" || aPrefName != this.pref) {
      return;
    }

    this._update();
  },

  _update() {
    if (Services.prefs.getBoolPref(this.pref)) {
      document.documentElement.setAttribute("extradragspace", "true");
    } else {
      document.documentElement.removeAttribute("extradragspace");
    }
  },
};
