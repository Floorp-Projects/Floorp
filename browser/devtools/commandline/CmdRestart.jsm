/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

let EXPORTED_SYMBOLS = [ ];

Cu.import("resource:///modules/devtools/gcli.jsm");
Cu.import("resource://gre/modules/Services.jsm");

/**
 * Restart command
 *
 * @param boolean nocache
 *        Disables loading content from cache upon restart.
 *
 * Examples :
 * >> restart
 * - restarts browser immediately
 * >> restart --nocache
 * - restarts immediately and starts Firefox without using cache
 */
gcli.addCommand({
  name: "restart",
  description: gcli.lookup("restartFirefoxDesc"),
  params: [
    {
      name: "nocache",
      type: "boolean",
      description: gcli.lookup("restartFirefoxNocacheDesc")
    }
  ],
  returnType: "string",
  exec: function Restart(args, context) {
    let canceled = Cc["@mozilla.org/supports-PRBool;1"]
                     .createInstance(Ci.nsISupportsPRBool);
    Services.obs.notifyObservers(canceled, "quit-application-requested", "restart");
    if (canceled.data) {
      return gcli.lookup("restartFirefoxRequestCancelled");
    }

    // disable loading content from cache.
    if (args.nocache) {
      Services.appinfo.invalidateCachesOnRestart();
    }

    // restart
    Cc['@mozilla.org/toolkit/app-startup;1']
      .getService(Ci.nsIAppStartup)
      .quit(Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart);
    return gcli.lookup("restartFirefoxRestarting");
  }
});
