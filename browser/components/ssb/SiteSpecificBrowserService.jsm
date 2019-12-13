/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["SiteSpecificBrowserService", "SSBCommandLineHandler"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const SiteSpecificBrowserService = {
  /**
   * Given a URI launches the SSB UI to display it.
   *
   * @param {nsIURI} uri the URI to display.
   */
  launchFromURI(uri) {
    if (!this.isEnabled) {
      throw new Error("Site specific browsing is disabled.");
    }

    if (!uri.schemeIs("https")) {
      throw new Error(
        "Site specific browsers can only be opened for secure sites."
      );
    }

    let sa = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    let uristr = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString
    );
    uristr.data = uri.spec;
    sa.appendElement(uristr);
    Services.ww.openWindow(
      null,
      "chrome://browser/content/ssb/ssb.html",
      "_blank",
      "chrome,dialog=no,all",
      sa
    );
  },
};

XPCOMUtils.defineLazyPreferenceGetter(
  SiteSpecificBrowserService,
  "isEnabled",
  "browser.ssb.enabled",
  false
);

class SSBCommandLineHandler {
  /* nsICommandLineHandler */
  handle(cmdLine) {
    if (!SiteSpecificBrowserService.isEnabled) {
      return;
    }

    let site = cmdLine.handleFlagWithParam("ssb", false);
    if (site) {
      cmdLine.preventDefault = true;

      try {
        let fixupInfo = Services.uriFixup.getFixupURIInfo(
          site,
          Services.uriFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS
        );

        let uri = fixupInfo.preferredURI;
        if (!uri) {
          dump(`Unable to parse '${site}' as a URI.\n`);
          return;
        }

        if (fixupInfo.fixupChangedProtocol && uri.schemeIs("http")) {
          uri = uri
            .mutate()
            .setScheme("https")
            .finalize();
        }
        SiteSpecificBrowserService.launchFromURI(uri);
      } catch (e) {
        dump(`Unable to parse '${site}' as a URI: ${e}\n`);
      }
    }
  }

  get helpInfo() {
    return "  --ssb <uri>        Open a site specific browser for <uri>.\n";
  }
}

SSBCommandLineHandler.prototype.QueryInterface = ChromeUtils.generateQI([
  Ci.nsICommandLineHandler,
]);
