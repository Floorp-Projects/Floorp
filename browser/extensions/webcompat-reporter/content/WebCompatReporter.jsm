/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["WebCompatReporter"];

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "PageActions",
  "resource:///modules/PageActions.jsm");

XPCOMUtils.defineLazyGetter(this, "wcStrings", function() {
  return Services.strings.createBundle(
    "chrome://webcompat-reporter/locale/webcompat.properties");
});

// Gather values for interesting details we want to appear in reports.
let details = {};
XPCOMUtils.defineLazyPreferenceGetter(details, "gfx.webrender.all", "gfx.webrender.all", false);
XPCOMUtils.defineLazyPreferenceGetter(details, "gfx.webrender.blob-images", "gfx.webrender.blob-images", true);
XPCOMUtils.defineLazyPreferenceGetter(details, "gfx.webrender.enabled", "gfx.webrender.enabled", false);
XPCOMUtils.defineLazyPreferenceGetter(details, "image.mem.shared", "image.mem.shared", true);
details.buildID = Services.appinfo.appBuildID;
details.channel = AppConstants.MOZ_UPDATE_CHANNEL;

Object.defineProperty(details, "blockList", {
  // We don't want this property to end up in the stringified details
  enumerable: false,
  get() {
    let trackingTable = Services.prefs.getCharPref("urlclassifier.trackingTable");
    // If content-track-digest256 is in the tracking table,
    // the user has enabled the strict list.
    return trackingTable.includes("content") ? "strict" : "basic";
  }
});

if (AppConstants.platform == "linux") {
  XPCOMUtils.defineLazyPreferenceGetter(details, "layers.acceleration.force-enabled", "layers.acceleration.force-enabled", false);
}

let WebCompatReporter = {
  get endpoint() {
    return Services.urlFormatter.formatURLPref(
      "extensions.webcompat-reporter.newIssueEndpoint");
  },

  init() {
    PageActions.addAction(new PageActions.Action({
      id: "webcompat-reporter-button",
      title: wcStrings.GetStringFromName("wc-reporter.label2"),
      iconURL: "chrome://webcompat-reporter/skin/lightbulb.svg",
      labelForHistogram: "webcompat",
      onCommand: (e) => this.reportIssue(e.target.ownerGlobal),
      onLocationChange: (window) => this.onLocationChange(window)
    }));
  },

  uninit() {
    let action = PageActions.actionForID("webcompat-reporter-button");
    action.remove();
  },

  onLocationChange(window) {
    let action = PageActions.actionForID("webcompat-reporter-button");
    let scheme = window.gBrowser.currentURI.scheme;
    let isReportable = ["http", "https"].includes(scheme);
    action.setDisabled(!isReportable, window);
  },

  // This method injects a framescript that should send back a screenshot blob
  // of the top-level window of the currently selected tab, and some other details
  // about the tab (url, tracking protection + mixed content blocking status)
  // resolved as a Promise.
  getScreenshot(gBrowser) {
    const FRAMESCRIPT = "chrome://webcompat-reporter/content/tab-frame.js";
    const TABDATA_MESSAGE = "WebCompat:SendTabData";

    return new Promise((resolve) => {
      let mm = gBrowser.selectedBrowser.messageManager;
      mm.loadFrameScript(FRAMESCRIPT, false);

      mm.addMessageListener(TABDATA_MESSAGE, function receiveFn(message) {
        mm.removeMessageListener(TABDATA_MESSAGE, receiveFn);
        resolve([gBrowser, message.json]);
      });
    });
  },

  // This should work like so:
  // 1) set up listeners for a new webcompat.com tab, and open it, passing
  //    along the current URI
  // 2) if we successfully got a screenshot from getScreenshot,
  //    inject a frame script that will postMessage it to webcompat.com
  //    so it can show a preview to the user and include it in FormData
  // Note: openWebCompatTab arguments are passed in as an array because they
  // are the result of a promise resolution.
  openWebCompatTab([gBrowser, tabData]) {
    const SCREENSHOT_MESSAGE = "WebCompat:SendScreenshot";
    const FRAMESCRIPT = "chrome://webcompat-reporter/content/wc-frame.js";
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    const WEBCOMPAT_ORIGIN = new win.URL(WebCompatReporter.endpoint).origin;

    // Grab the relevant tab environment details that might change per site
    details["mixed active content blocked"] = tabData.hasMixedActiveContentBlocked;
    details["mixed passive content blocked"] = tabData.hasMixedDisplayContentBlocked;
    details["tracking content blocked"] = tabData.hasTrackingContentBlocked ?
      `true (${details.blockList})` : "false";

      // question: do i add a label for basic vs strict?

    let params = new URLSearchParams();
    params.append("url", `${tabData.url}`);
    params.append("src", "desktop-reporter");
    params.append("details", JSON.stringify(details));

    if (details["gfx.webrender.all"] || details["gfx.webrender.enabled"]) {
      params.append("label", "type-webrender-enabled");
    }

    if (tabData.hasTrackingContentBlocked) {
      params.append("label", `type-tracking-protection-${details.blockList}`);
    }

    let tab = gBrowser.loadOneTab(
      `${WebCompatReporter.endpoint}?${params}`,
      {inBackground: false, triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal()});

    // If we successfully got a screenshot blob, add a listener to know when
    // the new tab is loaded before sending it over.
    if (tabData && tabData.blob) {
      let browser = gBrowser.getBrowserForTab(tab);
      let loadedListener = {
        QueryInterface: ChromeUtils.generateQI(["nsIWebProgressListener",
          "nsISupportsWeakReference"]),
        onStateChange(webProgress, request, flags, status) {
          let isStopped = flags & Ci.nsIWebProgressListener.STATE_STOP;
          let isNetwork = flags & Ci.nsIWebProgressListener.STATE_IS_NETWORK;
          if (isStopped && isNetwork && webProgress.isTopLevel) {
            let location;
            try {
              location = request.QueryInterface(Ci.nsIChannel).URI;
            } catch (ex) {}

            if (location && location.prePath === WEBCOMPAT_ORIGIN) {
              let mm = gBrowser.selectedBrowser.messageManager;
              mm.loadFrameScript(FRAMESCRIPT, false);
              mm.sendAsyncMessage(SCREENSHOT_MESSAGE, {
                screenshot: tabData.blob,
                origin: WEBCOMPAT_ORIGIN
              });

              browser.removeProgressListener(this);
            }
          }
        }
      };

      browser.addProgressListener(loadedListener);
    }
  },

  reportIssue(global) {
    this.getScreenshot(global.gBrowser).then(this.openWebCompatTab)
                                       .catch(Cu.reportError);
  }
};
