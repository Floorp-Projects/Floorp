/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

/**
 * This code exists to be a "grab bag" of global code that needs to be
 * loaded per B2G process, but doesn't need to directly interact with
 * web content.
 *
 * (It's written as an XPCOM service because it needs to watch
 * app-startup.)
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/XPCOMUtils.jsm');

// Preloading the CSP jsm in this process early on.
Cu.import("resource://gre/modules/CSPUtils.jsm");

function debug(msg) {
  log(msg);
}
function log(msg) {
  // This file implements console.log(), so use dump().
  //dump('ProcessGlobal: ' + msg + '\n');
}

const gFactoryResetFile = "/persist/__post_reset_cmd__";

function ProcessGlobal() {}
ProcessGlobal.prototype = {
  classID: Components.ID('{1a94c87a-5ece-4d11-91e1-d29c29f21b28}'),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  wipeDir: function(path) {
    log("wipeDir " + path);
    let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    dir.initWithPath(path);
    if (!dir.exists() || !dir.isDirectory()) {
      return;
    }
    let entries = dir.directoryEntries;
    while (entries.hasMoreElements()) {
      let file = entries.getNext().QueryInterface(Ci.nsIFile);
      log("Deleting " + file.path);
      try {
        file.remove(true);
      } catch(e) {}
    }
  },

  processWipeFile: function(text) {
    log("processWipeFile " + text);
    let lines = text.split("\n");
    lines.forEach((line) => {
      log(line);
      let params = line.split(" ");
      if (params[0] == "wipe") {
        this.wipeDir(params[1]);
      }
    });
  },

  cleanupAfterFactoryReset: function() {
    log("cleanupAfterWipe start");

    let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    file.initWithPath(gFactoryResetFile);
    if (!file.exists()) {
      debug("Nothing to wipe.")
      return;
    }

    Cu.import("resource://gre/modules/osfile.jsm");
    let promise = OS.File.read(gFactoryResetFile);
    promise.then(
      (array) => {
        file.remove(false);
        let decoder = new TextDecoder();
        this.processWipeFile(decoder.decode(array));
      }
    );

    log("cleanupAfterWipe end.");
  },

  observe: function pg_observe(subject, topic, data) {
    switch (topic) {
    case 'app-startup': {
      Services.obs.addObserver(this, 'console-api-log-event', false);
      let inParent = Cc["@mozilla.org/xre/app-info;1"]
                       .getService(Ci.nsIXULRuntime)
                       .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
      if (inParent) {
        let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
                     .getService(Ci.nsIMessageListenerManager);
        ppmm.addMessageListener("getProfD", function(message) {
          return Services.dirsvc.get("ProfD", Ci.nsIFile).path;
        });

        this.cleanupAfterFactoryReset();
      }
      break;
    }
    case 'console-api-log-event': {
      // Pipe `console` log messages to the nsIConsoleService which
      // writes them to logcat on Gonk.
      let message = subject.wrappedJSObject;
      let prefix = ('Content JS ' + message.level.toUpperCase() +
                    ' at ' + message.filename + ':' + message.lineNumber +
                    ' in ' + (message.functionName || 'anonymous') + ': ');
      Services.console.logStringMessage(prefix + Array.join(message.arguments,
                                                            ' '));
      break;
    }
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ProcessGlobal]);
