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
  switch (prefSvc.getPrefType(name)) {
  case Ci.nsIPrefBranch.PREF_STRING:
    return prefSvc.getComplexValue(name, Ci.nsISupportsString).data;

  case Ci.nsIPrefBranch.PREF_INT:
    return prefSvc.getIntPref(name);

  case Ci.nsIPrefBranch.PREF_BOOL:
    return prefSvc.getBoolPref(name);

  case Ci.nsIPrefBranch.PREF_INVALID:
    return defaultValue;

  default:
    // This should never happen.
    throw new Error("Error getting pref " + name +
                    "; its value's type is " +
                    prefSvc.getPrefType(name) +
                    ", which I don't know " +
                    "how to handle.");
  }
}
exports.get = get;

function set(name, value) {
  var prefType;
  if (typeof value != "undefined" && value != null)
    prefType = value.constructor.name;

  switch (prefType) {
  case "String":
    {
      var string = Cc["@mozilla.org/supports-string;1"].
                   createInstance(Ci.nsISupportsString);
      string.data = value;
      prefSvc.setComplexValue(name, Ci.nsISupportsString, string);
    }
    break;

  case "Number":
    // We throw if the number is outside the range or not an integer, since
    // the result will not be what the consumer wanted to store.
    if (value > MAX_INT || value < MIN_INT)
      throw new Error("you cannot set the " + name +
                      " pref to the number " + value +
                      ", as number pref values must be in the signed " +
                      "32-bit integer range -(2^31) to 2^31-1.  " +
                      "To store numbers outside that range, store " +
                      "them as strings.");
    if (value % 1 != 0)
      throw new Error("cannot store non-integer number: " + value);
    prefSvc.setIntPref(name, value);
    break;

  case "Boolean":
    prefSvc.setBoolPref(name, value);
    break;

  default:
    throw new Error("can't set pref " + name + " to value '" + value +
                    "'; it isn't a string, integer, or boolean");
  }
}
exports.set = set;

function has(name) {
  return (prefSvc.getPrefType(name) != Ci.nsIPrefBranch.PREF_INVALID);
}
exports.has = has;

function keys(root) {
  return prefSvc.getChildList(root);
}
exports.keys = keys;

function isSet(name) {
  return (has(name) && prefSvc.prefHasUserValue(name));
}
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

