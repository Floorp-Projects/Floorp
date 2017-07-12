/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["WebCompatReporter"];

let { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const TABLISTENER_JSM = "chrome://webcompat-reporter/content/TabListener.jsm";
const WIDGET_ID = "webcompat-reporter-button";

XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm");

XPCOMUtils.defineLazyGetter(this, "wcStrings", function() {
  return Services.strings.createBundle(
    "chrome://webcompat-reporter/locale/webcompat.properties");
});

XPCOMUtils.defineLazyGetter(this, "wcStyleURI", function() {
  return Services.io.newURI("chrome://webcompat-reporter/skin/lightbulb.css");
});

let WebCompatReporter = {
  get endpoint() {
    return Services.urlFormatter.formatURLPref(
      "extensions.webcompat-reporter.newIssueEndpoint");
  },

  init() {
    let styleSheetService = Cc["@mozilla.org/content/style-sheet-service;1"]
      .getService(Ci.nsIStyleSheetService);
    this._sheetType = styleSheetService.AUTHOR_SHEET;
    this._cachedSheet = styleSheetService.preloadSheet(wcStyleURI,
                                                       this._sheetType);

    XPCOMUtils.defineLazyModuleGetter(this, "TabListener", TABLISTENER_JSM);

    CustomizableUI.createWidget({
      id: WIDGET_ID,
      label: wcStrings.GetStringFromName("wc-reporter.label"),
      tooltiptext: wcStrings.GetStringFromName("wc-reporter.tooltip"),
      defaultArea: CustomizableUI.AREA_PANEL,
      disabled: true,
      onCommand: (e) => this.reportIssue(e.target.ownerDocument),
    });

    for (let win of CustomizableUI.windows) {
      this.onWindowOpened(win);
    }

    CustomizableUI.addListener(this);
  },

  onWindowOpened(win) {
    // Attach stylesheet for the button icon.
    win.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils)
      .addSheet(this._cachedSheet, this._sheetType);
    // Attach listeners to new window.
    win._webcompatReporterTabListener = new this.TabListener(win);
  },

  onWindowClosed(win) {
    if (win._webcompatReporterTabListener) {
      win._webcompatReporterTabListener.removeListeners();
      delete win._webcompatReporterTabListener;
    }
  },

  uninit() {
    CustomizableUI.destroyWidget(WIDGET_ID);

    for (let win of CustomizableUI.windows) {
      this.onWindowClosed(win);

      win.QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIDOMWindowUtils)
        .removeSheet(wcStyleURI, this._sheetType);
    }

    CustomizableUI.removeListener(this);

    if (Cu.isModuleLoaded(TABLISTENER_JSM)) {
      Cu.unload(TABLISTENER_JSM);
    }
  },

  // This method injects a framescript that should send back a screenshot blob
  // of the top-level window of the currently selected tab, resolved as a
  // Promise.
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

    let tab = gBrowser.loadOneTab(
      `${WebCompatReporter.endpoint}?url=${encodeURIComponent(tabData.url)}&src=desktop-reporter`,
      {inBackground: false, triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal()});

    // If we successfully got a screenshot blob, add a listener to know when
    // the new tab is loaded before sending it over.
    if (tabData && tabData.blob) {
      let browser = gBrowser.getBrowserForTab(tab);
      let loadedListener = {
        QueryInterface: XPCOMUtils.generateQI(["nsIWebProgressListener",
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

  reportIssue(xulDoc) {
    this.getScreenshot(xulDoc.defaultView.gBrowser).then(this.openWebCompatTab)
                                                   .catch(Cu.reportError);
  }
};
