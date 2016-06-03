/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Cc, Ci, Cu, CC} = require("chrome");
const protocol = require("devtools/shared/protocol");
const {Arg, method, RetVal} = protocol;
const Services = require("Services");
const {preferenceSpec} = require("devtools/shared/specs/preference");

exports.register = function (handle) {
  handle.addGlobalActor(PreferenceActor, "preferenceActor");
};

exports.unregister = function (handle) {
};

var PreferenceActor = exports.PreferenceActor = protocol.ActorClassWithSpec(preferenceSpec, {
  typeName: "preference",

  getBoolPref: function (name) {
    return Services.prefs.getBoolPref(name);
  },

  getCharPref: function (name) {
    return Services.prefs.getCharPref(name);
  },

  getIntPref: function (name) {
    return Services.prefs.getIntPref(name);
  },

  getAllPrefs: function () {
    let prefs = {};
    Services.prefs.getChildList("").forEach(function (name, index) {
      // append all key/value pairs into a huge json object.
      try {
        let value;
        switch (Services.prefs.getPrefType(name)) {
          case Ci.nsIPrefBranch.PREF_STRING:
            value = Services.prefs.getCharPref(name);
            break;
          case Ci.nsIPrefBranch.PREF_INT:
            value = Services.prefs.getIntPref(name);
            break;
          case Ci.nsIPrefBranch.PREF_BOOL:
            value = Services.prefs.getBoolPref(name);
            break;
          default:
        }
        prefs[name] = {
          value: value,
          hasUserValue: Services.prefs.prefHasUserValue(name)
        };
      } catch (e) {
        // pref exists but has no user or default value
      }
    });
    return prefs;
  },

  setBoolPref: function (name, value) {
    Services.prefs.setBoolPref(name, value);
    Services.prefs.savePrefFile(null);
  },

  setCharPref: function (name, value) {
    Services.prefs.setCharPref(name, value);
    Services.prefs.savePrefFile(null);
  },

  setIntPref: function (name, value) {
    Services.prefs.setIntPref(name, value);
    Services.prefs.savePrefFile(null);
  },

  clearUserPref: function (name) {
    Services.prefs.clearUserPref(name);
    Services.prefs.savePrefFile(null);
  },
});
