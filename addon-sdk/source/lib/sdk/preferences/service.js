/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

// The minimum and maximum integers that can be set as preferences.
// The range of valid values is narrower than the range of valid JS values
// because the native preferences code treats integers as NSPR PRInt32s,
// which are 32-bit signed integers on all platforms.
const MAX_INT = 0x7FFFFFFF;
const MIN_INT = -0x80000000;

const {Cc,Ci,Cr} = require("chrome");

const prefService = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefService);
const prefSvc = prefService.getBranch(null);
const defaultBranch = prefService.getDefaultBranch(null);

const { Preferences } = require("resource://gre/modules/Preferences.jsm");
const prefs = new Preferences({});

function Branch(branchName) {
  function getPrefKeys() {
    return keys(branchName).map(function(key) {
      return key.replace(branchName, "");
    });
  }

  return Proxy.create({
    get: function(receiver, pref) {
      return get(branchName + pref);
    },
    set: function(receiver, pref, val) {
      set(branchName + pref, val);
    },
    delete: function(pref) {
      reset(branchName + pref);
      return true;
    },
    has: function hasPrefKey(pref) {
      return has(branchName + pref)
    },
    hasOwn: function(pref) {
      return has(branchName + pref)
    },
    getPropertyDescriptor: function(name) {
      return {
        value: get(branchName + name)
      };
    },
    enumerate: getPrefKeys,
    keys: getPrefKeys
  }, Branch.prototype);
}

function get(name, defaultValue) {
  return prefs.get(name, defaultValue);
}
exports.get = get;


function set(name, value) {
  var prefType;
  if (typeof value != "undefined" && value != null)
    prefType = value.constructor.name;

  switch (prefType) {
  case "Number":
    if (value % 1 != 0)
      throw new Error("cannot store non-integer number: " + value);
  }

  prefs.set(name, value);
}
exports.set = set;

const has = prefs.has.bind(prefs)
exports.has = has;

function keys(root) {
  return prefSvc.getChildList(root);
}
exports.keys = keys;

const isSet = prefs.isSet.bind(prefs);
exports.isSet = isSet;

function reset(name) {
  try {
    prefSvc.clearUserPref(name);
  }
  catch (e) {
    // The pref service throws NS_ERROR_UNEXPECTED when the caller tries
    // to reset a pref that doesn't exist or is already set to its default
    // value.  This interface fails silently in those cases, so callers
    // can unconditionally reset a pref without having to check if it needs
    // resetting first or trap exceptions after the fact.  It passes through
    // other exceptions, however, so callers know about them, since we don't
    // know what other exceptions might be thrown and what they might mean.
    if (e.result != Cr.NS_ERROR_UNEXPECTED) {
      throw e;
    }
  }
}
exports.reset = reset;

function getLocalized(name, defaultValue) {
  let value = null;
  try {
    value = prefSvc.getComplexValue(name, Ci.nsIPrefLocalizedString).data;
  }
  finally {
    return value || defaultValue;
  }
}
exports.getLocalized = getLocalized;

function setLocalized(name, value) {
  // We can't use `prefs.set` here as we have to use `getDefaultBranch`
  // (instead of `getBranch`) in order to have `mIsDefault` set to true, here:
  // http://mxr.mozilla.org/mozilla-central/source/modules/libpref/src/nsPrefBranch.cpp#233
  // Otherwise, we do not enter into this expected condition:
  // http://mxr.mozilla.org/mozilla-central/source/modules/libpref/src/nsPrefBranch.cpp#244
  defaultBranch.setCharPref(name, value);
}
exports.setLocalized = setLocalized;

exports.Branch = Branch;

