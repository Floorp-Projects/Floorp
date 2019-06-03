/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, XPCOMUtils */

var {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "l10nStrings", function() {
  return Services.strings.createBundle(
    "chrome://report-site-issue/locale/webcompat.properties");
});

let l10nManifest;

this.l10n = class extends ExtensionAPI {
  onShutdown(isAppShutdown) {
    if (!isAppShutdown && l10nManifest) {
      Components.manager.removeBootstrappedManifestLocation(l10nManifest);
    }
  }
  getAPI(context) {
    // Until we move to Fluent (bug 1446164), we're stuck with
    // chrome.manifest for handling localization since its what the
    // build system can handle for localized repacks.
    if (context.extension.rootURI instanceof Ci.nsIJARURI) {
      l10nManifest = context.extension.rootURI.JARFile
                            .QueryInterface(Ci.nsIFileURL).file;
    } else if (context.extension.rootURI instanceof Ci.nsIFileURL) {
      l10nManifest = context.extension.rootURI.file;
    }

    if (l10nManifest) {
      Components.manager.addBootstrappedManifestLocation(l10nManifest);
    } else {
      Cu.reportError("Cannot find webcompat reporter chrome.manifest for registring translated strings");
    }

    return {
      l10n: {
        getMessage(name) {
          try {
            return Promise.resolve(l10nStrings.GetStringFromName(name));
          } catch (e) {
            return Promise.reject(e);
          }
        },
      },
    };
  }
};
