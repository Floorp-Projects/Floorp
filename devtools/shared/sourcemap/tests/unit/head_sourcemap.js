/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

function doesNotThrow(f) {
  try {
    f();
  } catch(e) {
    ok(false, e + e.stack);
  }
}

var assert = this;
