/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A wrapper for Services.prefs that compares our shim content
// implementation with the real service.

// We assume we're loaded in a global where Services was already loaded.
/* globals isDeeply, Services */

"use strict";

function setMethod(methodName, prefName, value) {
  let savedException;
  let prefThrew = false;
  try {
    Services.prefs[methodName](prefName, value);
  } catch (e) {
    prefThrew = true;
    savedException = e;
  }

  let realThrew = false;
  try {
    SpecialPowers[methodName](prefName, value);
  } catch (e) {
    realThrew = true;
    savedException = e;
  }

  is(prefThrew, realThrew, methodName + " [throw check]");
  if (prefThrew || realThrew) {
    throw savedException;
  }
}

function getMethod(methodName, prefName) {
  let prefThrew = false;
  let prefValue = undefined;
  let savedException;
  try {
    prefValue = Services.prefs[methodName](prefName);
  } catch (e) {
    prefThrew = true;
    savedException = e;
  }

  let realValue = undefined;
  let realThrew = false;
  try {
    realValue = SpecialPowers[methodName](prefName);
  } catch (e) {
    realThrew = true;
    savedException = e;
  }

  is(prefThrew, realThrew, methodName + " [throw check]");
  isDeeply(prefValue, realValue, methodName + " [equality]");
  if (prefThrew || realThrew) {
    throw savedException;
  }

  return prefValue;
}

var WrappedPrefs = {};

for (let method of ["getPrefType", "getBoolPref", "getCharPref", "getIntPref",
                    "clearUserPref"]) {
  WrappedPrefs[method] = getMethod.bind(null, method);
}

for (let method of ["setBoolPref", "setCharPref", "setIntPref"]) {
  WrappedPrefs[method] = setMethod.bind(null, method);
}

// Silence eslint.
exports.WrappedPrefs = WrappedPrefs;
