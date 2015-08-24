/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

this.EXPORTED_SYMBOLS = ["ScriptPreloader"];

function debug(aMsg) {
  //dump("--*-- ScriptPreloader: " + aMsg + "\n");
}

this.ScriptPreloader = {
#ifdef MOZ_B2G
  _enabled: true,
#else
  _enabled: false,
#endif

  preload: function(aApp, aManifest) {
    debug("Preloading " + aApp.origin);
    let deferred = Promise.defer();

    if (!this._enabled) {
      deferred.resolve();
      return deferred.promise;
    }

    if (aManifest.precompile &&
        Array.isArray(aManifest.precompile) &&
        aManifest.precompile.length > 0) {
      let origin = Services.io.newURI(aApp.origin, null, null);
      let toLoad = aManifest.precompile.length;
      let principal =
        Services.scriptSecurityManager
                .createCodebasePrincipal(origin, {appId: aApp.localId});

      aManifest.precompile.forEach((aPath) => {
        let uri = Services.io.newURI(aPath, null, origin);
        debug("Script to compile: " + uri.spec);
        try {
          Services.scriptloader.precompileScript(uri, principal,
            (aSubject, aTopic, aData) => {
              let uri = aSubject.QueryInterface(Ci.nsIURI);
              debug("Done compiling " + uri.spec);

              toLoad--;
              if (toLoad == 0) {
                deferred.resolve();
              }
            });
        } catch (e) {
          // Resolve the promise if precompileScript throws.
          deferred.resolve();
        }
      });
    } else {
      // The precompile field is not an array, let the developer know.
      // We don't want to have to enable debug for that to show up.
      if (aManifest.precompile) {
        Cu.reportError("ASM.JS compilation failed: the 'precompile' manifest " +
                       "property should be an array of script uris.\n");
      }
      deferred.resolve();
    }

    return deferred.promise;
  }
}
