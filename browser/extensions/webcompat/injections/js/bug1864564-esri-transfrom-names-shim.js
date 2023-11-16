/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Bug 1864564 - Override esri._css.names containing -moz-prefixed css rules names
 * Webcompat issue #129144 - https://github.com/webcompat/web-bugs/issues/129144
 *
 * Esri library is applying -moz-transform to maps built with it, based on UA detection.
 * Since support for -moz-transform has been removed in
 * https://bugzilla.mozilla.org/show_bug.cgi?id=1855763, this results
 * in maps partially or incorrectly displayed. Overriding  esri._css.names
 * containing -moz-prefixed css properties to their unprefixed versions
 * fixes the issues.
 */

/* globals exportFunction, cloneInto */

console.info(
  "Overriding esri._css.names for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1864564 for details."
);

const transformNames = {
  transition: "transition",
  transform: "transform",
  transformName: "transform",
  origin: "transformOrigin",
  endEvent: "transitionend",
};

let esriGlobal;

Object.defineProperty(window.wrappedJSObject, "esri", {
  get: exportFunction(function () {
    if ("_css" in esriGlobal && "names" in esriGlobal._css) {
      esriGlobal._css.names = cloneInto(transformNames, esriGlobal);
    }
    return esriGlobal;
  }, window),

  set: exportFunction(function (value) {
    esriGlobal = value;
  }, window),
});
