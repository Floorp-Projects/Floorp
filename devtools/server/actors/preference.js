/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const { preferenceSpec } = require("devtools/shared/specs/preference");

const { PREF_STRING, PREF_INT, PREF_BOOL } = Services.prefs;

function ensurePrefType(name, expectedType) {
  const type = Services.prefs.getPrefType(name);
  if (type !== expectedType) {
    throw new Error(`preference is not of the right type: ${name}`);
  }
}

/**
 * Normally the preferences are set using Services.prefs, but this actor allows
 * a devtools client to set preferences on the debuggee. This is particularly useful
 * when remote debugging, and the preferences should persist to the remote target
 * and not to the client. If used for a local target, it effectively behaves the same
 * as using Services.prefs.
 *
 * This actor is used as a global-scoped actor, targeting the entire browser, not an
 * individual tab.
 */
var PreferenceActor = protocol.ActorClassWithSpec(preferenceSpec, {
  getTraits() {
    // The *Pref traits are used to know if remote-debugging bugs related to
    // specific preferences are fixed on the server or if the client should set
    // default values for them. See the about:debugging module
    // runtime-default-preferences.js
    return {};
  },

  getBoolPref(name) {
    ensurePrefType(name, PREF_BOOL);
    return Services.prefs.getBoolPref(name);
  },

  getCharPref(name) {
    ensurePrefType(name, PREF_STRING);
    return Services.prefs.getCharPref(name);
  },

  getIntPref(name) {
    ensurePrefType(name, PREF_INT);
    return Services.prefs.getIntPref(name);
  },

  getAllPrefs() {
    const prefs = {};
    Services.prefs.getChildList("").forEach(function(name, index) {
      // append all key/value pairs into a huge json object.
      try {
        let value;
        switch (Services.prefs.getPrefType(name)) {
          case PREF_STRING:
            value = Services.prefs.getCharPref(name);
            break;
          case PREF_INT:
            value = Services.prefs.getIntPref(name);
            break;
          case PREF_BOOL:
            value = Services.prefs.getBoolPref(name);
            break;
          default:
        }
        prefs[name] = {
          value,
          hasUserValue: Services.prefs.prefHasUserValue(name),
        };
      } catch (e) {
        // pref exists but has no user or default value
      }
    });
    return prefs;
  },

  setBoolPref(name, value) {
    Services.prefs.setBoolPref(name, value);
    Services.prefs.savePrefFile(null);
  },

  setCharPref(name, value) {
    Services.prefs.setCharPref(name, value);
    Services.prefs.savePrefFile(null);
  },

  setIntPref(name, value) {
    Services.prefs.setIntPref(name, value);
    Services.prefs.savePrefFile(null);
  },

  clearUserPref(name) {
    Services.prefs.clearUserPref(name);
    Services.prefs.savePrefFile(null);
  },
});

exports.PreferenceActor = PreferenceActor;
