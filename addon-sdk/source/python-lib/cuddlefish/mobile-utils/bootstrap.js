/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

var { log } = console;

function startup(data, reason) {
  // This code allow to make all stdIO work
  try {
    Cu.import("resource://gre/modules/ctypes.jsm");
    let libdvm = ctypes.open("libdvm.so");
    let dvmStdioConverterStartup;
    // Starting with Android ICS, dalvik uses C++.
    // So that the symbol isn't a simple C one
    try {
      dvmStdioConverterStartup = libdvm.declare("_Z24dvmStdioConverterStartupv", ctypes.default_abi, ctypes.bool);
    }
    catch(e) {
      // Otherwise, before ICS, it was a pure C library
      dvmStdioConverterStartup = libdvm.declare("dvmStdioConverterStartup", ctypes.default_abi, ctypes.void_t);
    }
    dvmStdioConverterStartup();
    log("MU: console redirected to adb logcat.");
  } catch(e) {
    Cu.reportError("MU: unable to execute jsctype hack: "+e);
  }

  try {
    let QuitObserver = {
      observe: function (aSubject, aTopic, aData) {
        Services.obs.removeObserver(QuitObserver, "quit-application");
        dump("MU: APPLICATION-QUIT\n");
      }
    };
    Services.obs.addObserver(QuitObserver, "quit-application");
    log("MU: ready to watch firefox exit.");
  } catch(e) {
    log("MU: unable to register quit-application observer: " + e);
  }
}

function install() {}
function shutdown() {}
