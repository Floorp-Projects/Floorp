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
 * The Original Code is Preferences.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Myk Melez <myk@mozilla.org>
 *   Daniel Aquino <mr.danielaquino@gmail.com>
 *   Atul Varma <atul@mozilla.com>
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

var jsm = {}; Cu.import("resource://gre/modules/XPCOMUtils.jsm", jsm);
var XPCOMUtils = jsm.XPCOMUtils;

// The minimum and maximum integers that can be set as preferences.
// The range of valid values is narrower than the range of valid JS values
// because the native preferences code treats integers as NSPR PRInt32s,
// which are 32-bit signed integers on all platforms.
const MAX_INT = Math.pow(2, 31) - 1;
const MIN_INT = -MAX_INT;

var prefSvc = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefService).getBranch(null);

var get = exports.get = function get(name, defaultValue) {
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
};

var set = exports.set = function set(name, value) {
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
    // We throw if the number is outside the range, since the result
    // will never be what the consumer wanted to store, but we only warn
    // if the number is non-integer, since the consumer might not mind
    // the loss of precision.
    if (value > MAX_INT || value < MIN_INT)
      throw new Error("you cannot set the " + name +
                      " pref to the number " + value +
                      ", as number pref values must be in the signed " +
                      "32-bit integer range -(2^31-1) to 2^31-1.  " +
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
                    "'; it isn't a String, Number, or Boolean");
  }
};

var has = exports.has = function has(name) {
  return (prefSvc.getPrefType(name) != Ci.nsIPrefBranch.PREF_INVALID);
};

var isSet = exports.isSet = function isSet(name) {
  return (has(name) && prefSvc.prefHasUserValue(name));
};

var reset = exports.reset = function reset(name) {
  try {
    prefSvc.clearUserPref(name);
  } catch (e if e.result == Cr.NS_ERROR_UNEXPECTED) {
    // The pref service throws NS_ERROR_UNEXPECTED when the caller tries
    // to reset a pref that doesn't exist or is already set to its default
    // value.  This interface fails silently in those cases, so callers
    // can unconditionally reset a pref without having to check if it needs
    // resetting first or trap exceptions after the fact.  It passes through
    // other exceptions, however, so callers know about them, since we don't
    // know what other exceptions might be thrown and what they might mean.
  }
};
