/* global Services, Preferences, EventEmitter, XPCOMUtils */
/* exported NewTabPrefsProvider */

"use strict";

this.EXPORTED_SYMBOLS = ["NewTabPrefsProvider"];

const {interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "EventEmitter", function() {
  const {EventEmitter} = Cu.import("resource://gre/modules/devtools/event-emitter.js", {});
  return EventEmitter;
});

// Supported prefs and data type
const gPrefsMap = new Map([
  ["browser.newtabpage.enabled", "bool"],
  ["browser.newtabpage.enhanced", "bool"],
  ["browser.newtabpage.pinned", "str"],
  ["intl.locale.matchOS", "bool"],
  ["general.useragent.locale", "localized"],
]);

let PrefsProvider = function PrefsProvider() {
  EventEmitter.decorate(this);
};

PrefsProvider.prototype = {

  observe(subject, topic, data) { // jshint ignore:line
    if (topic === "nsPref:changed") {
      if (gPrefsMap.has(data)) {
        switch (gPrefsMap.get(data)) {
          case "bool":
            this.emit(data, Preferences.get(data, false));
            break;
          case "str":
            this.emit(data, Preferences.get(data, ""));
            break;
          case "localized":
            try {
              this.emit(data, Preferences.get(data, "", Ci.nsIPrefLocalizedString));
            } catch (e) {
              this.emit(data, Preferences.get(data, ""));
            }
            break;
          default:
            this.emit(data);
            break;
        }
      }
    } else {
      Cu.reportError(new Error("NewTabPrefsProvider observing unknown topic"));
    }
  },

  get prefsMap() {
    return gPrefsMap;
  },

  init() {
    for (let pref of gPrefsMap.keys()) {
      Services.prefs.addObserver(pref, this, false);
    }
  },

  uninit() {
    for (let pref of gPrefsMap.keys()) {
      Services.prefs.removeObserver(pref, this, false);
    }
  }
};

/**
 * Singleton that serves as the default new tab pref provider for the grid.
 */
const gPrefs = new PrefsProvider();

let NewTabPrefsProvider = {
  prefs: gPrefs,
};
