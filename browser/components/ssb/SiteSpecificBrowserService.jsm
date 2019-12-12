/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A Site Specific Browser intends to allow the user to navigate through the
 * chosen site in the SSB UI. Any attempt to load something outside the site
 * should be loaded in a normal browser. In order to achieve this we have to use
 * various APIs to listen for attempts to load new content and take appropriate
 * action. Often this requires returning synchronous responses to method calls
 * in content processes and will require data about the SSB in order to respond
 * correctly. Here we implement an architecture to support that:
 *
 * In the main process the SiteSpecificBrowser class implements all the
 * functionality involved with managing an SSB. All content processes can
 * synchronously retrieve a matching SiteSpecificBrowserBase that has enough
 * data about the SSB in order to be able to respond to load requests
 * synchronously. To support this we give every SSB a unique ID (UUID based)
 * and the appropriate data is shared via sharedData. Once created the ID can be
 * used to retrieve the SiteSpecificBrowser instance in the main process or
 * SiteSpecificBrowserBase instance in any content process.
 */

var EXPORTED_SYMBOLS = [
  "SiteSpecificBrowserService",
  "SiteSpecificBrowserBase",
  "SiteSpecificBrowser",
  "SSBCommandLineHandler",
];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

function uuid() {
  return Cc["@mozilla.org/uuid-generator;1"]
    .getService(Ci.nsIUUIDGenerator)
    .generateUUID()
    .toString();
}

const sharedDataKey = id => `SiteSpecificBrowserBase:${id}`;

const IS_MAIN_PROCESS =
  Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_DEFAULT;

/**
 * Maintains an ID -> SSB mapping in the main process. Content processes should
 * use sharedData to get a SiteSpecificBrowserBase.
 *
 * We do not currently expire data from here so once created an SSB instance
 * lives for the lifetime of the application. The expectation is that the
 * numbers of different SSBs used will be low and the memory use will also
 * be low.
 */
const SSBMap = new Map();

/**
 * The base contains the data about an SSB instance needed in content processes.
 *
 * The only data needed currently is the URI used to launch the SSB.
 */
class SiteSpecificBrowserBase {
  /**
   * Creates a new SiteSpecificBrowserBase. Generally should only be called by
   * code within this module.
   *
   * @param {nsIURI} uri the base URI for the SSB.
   */
  constructor(uri) {
    this._uri = uri;
  }

  /**
   * Gets the SiteSpecifcBrowserBase for an ID. If this is the main process this
   * will instead return the SiteSpecificBrowser instance itself but generally
   * don't call this from the main process.
   *
   * The returned object is not "live" and will not be updated with any
   * configuration changes from the main process so do not cache this, get it
   * when needed and then discard.
   *
   * @param {string} id the SSB ID.
   * @return {SiteSpecificBrowserBase|null} the instance if it exists.
   */
  static get(id) {
    if (IS_MAIN_PROCESS) {
      return SiteSpecificBrowser.get(id);
    }

    let key = sharedDataKey(id);
    if (!Services.cpmm.sharedData.has(key)) {
      return null;
    }

    let uri = Services.io.newURI(Services.cpmm.sharedData.get(key));
    return new SiteSpecificBrowserBase(uri);
  }

  /**
   * Checks whether the given URI is considered to be a part of this SSB or not.
   * Any URIs that return false should be loaded in a normal browser.
   *
   * @param {nsIURI} uri the URI to check.
   * @return {boolean} whether this SSB can load the URI.
   */
  canLoad(uri) {
    // Always allow loading about:blank as it is the initial page for iframes.
    if (uri.spec == "about:blank") {
      return true;
    }

    // A simplistic check. Is this a uri from the same origin.
    return uri.prePath == this._uri.prePath;
  }
}

/**
 * The SSB instance used in the main process.
 */
class SiteSpecificBrowser extends SiteSpecificBrowserBase {
  /**
   * Creates a new SiteSpecificBrowser. Generally should only be called by
   * code within this module.
   *
   * @param {string} id  the SSB's unique ID.
   * @param {nsIURI} uri the base URI for the SSB.
   */
  constructor(id, uri) {
    if (!IS_MAIN_PROCESS) {
      throw new Error(
        "SiteSpecificBrowser instances are only available in the main process."
      );
    }

    super(uri);
    this._id = id;

    // Cache the SSB for retrieval.
    SSBMap.set(id, this);

    // Cache the data that the content processes need.
    Services.ppmm.sharedData.set(sharedDataKey(id), this._uri.spec);
    Services.ppmm.sharedData.flush();
  }

  /**
   * Gets the SiteSpecifcBrowser for an ID. Can only be called from the main
   * process.
   *
   * @param {string} id the SSB ID.
   * @return {SiteSpecificBrowser|null} the instance if it exists.
   */
  static get(id) {
    if (!IS_MAIN_PROCESS) {
      throw new Error(
        "SiteSpecificBrowser instances are only available in the main process."
      );
    }

    return SSBMap.get(id);
  }

  /**
   * The SSB's ID.
   */
  get id() {
    return this._id;
  }

  /**
   * The default URI to load.
   */
  get startURI() {
    return this._uri;
  }

  /**
   * Launches a SSB by opening the necessary UI.
   */
  launch() {
    let sa = Cc["@mozilla.org/array;1"].createInstance(Ci.nsIMutableArray);
    let idstr = Cc["@mozilla.org/supports-string;1"].createInstance(
      Ci.nsISupportsString
    );
    idstr.data = this.id;
    sa.appendElement(idstr);
    Services.ww.openWindow(
      null,
      "chrome://browser/content/ssb/ssb.html",
      "_blank",
      "chrome,dialog=no,all",
      sa
    );
  }
}

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

    let ssb = new SiteSpecificBrowser(uuid(), uri);
    ssb.launch();
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
