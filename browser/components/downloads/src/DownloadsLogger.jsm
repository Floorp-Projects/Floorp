/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The contents of this file were copied almost entirely from
 * toolkit/identity/LogUtils.jsm. Until we've got a more generalized logging
 * mechanism for toolkit, I think this is going to be how we roll.
 */

"use strict";

this.EXPORTED_SYMBOLS = ["DownloadsLogger"];
const PREF_DEBUG = "browser.download.debug";

const Cu = Components.utils;
const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

this.DownloadsLogger = {
  _generateLogMessage: function _generateLogMessage(args) {
    // create a string representation of a list of arbitrary things
    let strings = [];

    for (let arg of args) {
      if (typeof arg === 'string') {
        strings.push(arg);
      } else if (arg === undefined) {
        strings.push('undefined');
      } else if (arg === null) {
        strings.push('null');
      } else {
        try {
          strings.push(JSON.stringify(arg, null, 2));
        } catch(err) {
          strings.push("<<something>>");
        }
      }
    };
    return 'Downloads: ' + strings.join(' ');
  },

  /**
   * log() - utility function to print a list of arbitrary things
   *
   * Enable with about:config pref browser.download.debug
   */
  log: function DL_log(...args) {
    let output = this._generateLogMessage(args);
    dump(output + "\n");

    // Additionally, make the output visible in the Error Console
    Services.console.logStringMessage(output);
  },

  /**
   * reportError() - report an error through component utils as well as
   * our log function
   */
  reportError: function DL_reportError(...aArgs) {
    // Report the error in the browser
    let output = this._generateLogMessage(aArgs);
    Cu.reportError(output);
    dump("ERROR:" + output + "\n");
    for (let frame = Components.stack.caller; frame; frame = frame.caller) {
      dump("\t" + frame + "\n");
    }
  }

};
