/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["NetErrorParent"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { PrivateBrowsingUtils } = ChromeUtils.import(
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
const { SessionStore } = ChromeUtils.import(
  "resource:///modules/sessionstore/SessionStore.jsm"
);
const { HomePage } = ChromeUtils.import("resource:///modules/HomePage.jsm");

const { TelemetryController } = ChromeUtils.import(
  "resource://gre/modules/TelemetryController.jsm"
);

const PREF_SSL_IMPACT_ROOTS = [
  "security.tls.version.",
  "security.ssl3.",
  "security.tls13.",
];

ChromeUtils.defineModuleGetter(
  this,
  "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gSerializationHelper",
  "@mozilla.org/network/serialization-helper;1",
  "nsISerializationHelper"
);

class CaptivePortalObserver {
  constructor(actor) {
    this.actor = actor;
    Services.obs.addObserver(this, "captive-portal-login-abort");
    Services.obs.addObserver(this, "captive-portal-login-success");
  }

  stop() {
    Services.obs.removeObserver(this, "captive-portal-login-abort");
    Services.obs.removeObserver(this, "captive-portal-login-success");
  }

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "captive-portal-login-abort":
      case "captive-portal-login-success":
        // Send a message to the content when a captive portal is freed
        // so that error pages can refresh themselves.
        this.actor.sendAsyncMessage("AboutNetErrorCaptivePortalFreed");
        break;
    }
  }
}

class NetErrorParent extends JSWindowActorParent {
  constructor() {
    super();
    this.captivePortalObserver = new CaptivePortalObserver(this);
  }

  didDestroy() {
    if (this.captivePortalObserver) {
      this.captivePortalObserver.stop();
    }
  }

  get browser() {
    return this.browsingContext.top.embedderElement;
  }

  getSecurityInfo(securityInfoAsString) {
    if (!securityInfoAsString) {
      return null;
    }

    let securityInfo = gSerializationHelper.deserializeObject(
      securityInfoAsString
    );
    securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);

    return securityInfo;
  }

  hasChangedCertPrefs() {
    let prefSSLImpact = PREF_SSL_IMPACT_ROOTS.reduce((prefs, root) => {
      return prefs.concat(Services.prefs.getChildList(root));
    }, []);
    for (let prefName of prefSSLImpact) {
      if (Services.prefs.prefHasUserValue(prefName)) {
        return true;
      }
    }

    return false;
  }

  async ReportBlockingError(bcID, scheme, host, port, path, xfoAndCspInfo) {
    // For reporting X-Frame-Options error and CSP: frame-ancestors errors, We
    // are collecting 4 pieces of information.
    // 1. The X-Frame-Options in the response header.
    // 2. The CSP: frame-ancestors in the response header.
    // 3. The URI of the frame who triggers this error.
    // 4. The top-level URI which loads the frame.
    //
    // We will exclude the query strings from the reporting URIs.
    //
    // More details about the data we send can be found in
    // https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/data/xfocsp-error-report-ping.html
    //

    let topBC = BrowsingContext.get(bcID).top;
    let topURI = topBC.currentWindowGlobal.documentURI;

    // Get the URLs without query strings.
    let frame_uri = `${scheme}//${host}${port == -1 ? "" : ":" + port}${path}`;
    let top_uri = `${topURI.scheme}//${topURI.hostPort}${topURI.filePath}`;

    TelemetryController.submitExternalPing(
      "xfocsp-error-report",
      {
        ...xfoAndCspInfo,
        frame_hostname: host,
        top_hostname: topURI.host,
        frame_uri,
        top_uri,
      },
      { addClientId: false, addEnvironment: false }
    );
  }

  /**
   * Return the default start page for the cases when the user's own homepage is
   * infected, so we can get them somewhere safe.
   */
  getDefaultHomePage(win) {
    let url = win.BROWSER_NEW_TAB_URL;
    if (PrivateBrowsingUtils.isWindowPrivate(win)) {
      return url;
    }
    url = HomePage.getDefault();
    // If url is a pipe-delimited set of pages, just take the first one.
    if (url.includes("|")) {
      url = url.split("|")[0];
    }
    return url;
  }

  /**
   * Re-direct the browser to the previous page or a known-safe page if no
   * previous page is found in history.  This function is used when the user
   * browses to a secure page with certificate issues and is presented with
   * about:certerror.  The "Go Back" button should take the user to the previous
   * or a default start page so that even when their own homepage is on a server
   * that has certificate errors, we can get them somewhere safe.
   */
  goBackFromErrorPage(win) {
    if (!win.gBrowser) {
      return;
    }

    let state = JSON.parse(SessionStore.getTabState(win.gBrowser.selectedTab));
    if (state.index == 1) {
      // If the unsafe page is the first or the only one in history, go to the
      // start page.
      win.gBrowser.loadURI(this.getDefaultHomePage(win), {
        triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      });
    } else {
      win.gBrowser.goBack();
    }
  }

  /**
   * This function does a canary request to a reliable, maintained endpoint, in
   * order to help network code detect a system-wide man-in-the-middle.
   */
  primeMitm(browser) {
    // If we already have a mitm canary issuer stored, then don't bother with the
    // extra request. This will be cleared on every update ping.
    if (Services.prefs.getStringPref("security.pki.mitm_canary_issuer", null)) {
      return;
    }

    let url = Services.prefs.getStringPref(
      "security.certerrors.mitm.priming.endpoint"
    );
    let request = new XMLHttpRequest({ mozAnon: true });
    request.open("HEAD", url);
    request.channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
    request.channel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;

    request.addEventListener("error", event => {
      // Make sure the user is still on the cert error page.
      if (!browser.documentURI.spec.startsWith("about:certerror")) {
        return;
      }

      let secInfo = request.channel.securityInfo.QueryInterface(
        Ci.nsITransportSecurityInfo
      );
      if (secInfo.errorCodeString != "SEC_ERROR_UNKNOWN_ISSUER") {
        return;
      }

      // When we get to this point there's already something deeply wrong, it's very likely
      // that there is indeed a system-wide MitM.
      if (secInfo.serverCert && secInfo.serverCert.issuerName) {
        // Grab the issuer of the certificate used in the exchange and store it so that our
        // network-level MitM detection code has a comparison baseline.
        Services.prefs.setStringPref(
          "security.pki.mitm_canary_issuer",
          secInfo.serverCert.issuerName
        );

        // MitM issues are sometimes caused by software not registering their root certs in the
        // Firefox root store. We might opt for using third party roots from the system root store.
        if (
          Services.prefs.getBoolPref(
            "security.certerrors.mitm.auto_enable_enterprise_roots"
          )
        ) {
          if (
            !Services.prefs.getBoolPref("security.enterprise_roots.enabled")
          ) {
            // Loading enterprise roots happens on a background thread, so wait for import to finish.
            BrowserUtils.promiseObserved("psm:enterprise-certs-imported").then(
              () => {
                if (browser.documentURI.spec.startsWith("about:certerror")) {
                  browser.reload();
                }
              }
            );

            Services.prefs.setBoolPref(
              "security.enterprise_roots.enabled",
              true
            );
            // Record that this pref was automatically set.
            Services.prefs.setBoolPref(
              "security.enterprise_roots.auto-enabled",
              true
            );
          }
        } else {
          // Need to reload the page to make sure network code picks up the canary issuer pref.
          browser.reload();
        }
      }
    });

    request.send(null);
  }

  displayOfflineSupportPage(supportPageSlug) {
    const AVAILABLE_PAGES = ["connection-not-secure", "time-errors"];
    if (!AVAILABLE_PAGES.includes(supportPageSlug)) {
      console.log(
        `[Not supported] Offline support is not yet available for ${supportPageSlug} errors.`
      );
      return;
    }

    let offlinePagePath = `chrome://browser/content/certerror/supportpages/${supportPageSlug}.html`;
    let triggeringPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
    this.browser.loadURI(offlinePagePath, { triggeringPrincipal });
  }

  receiveMessage(message) {
    switch (message.name) {
      case "Browser:EnableOnlineMode":
        // Reset network state and refresh the page.
        Services.io.offline = false;
        this.browser.reload();
        break;
      case "Browser:OpenCaptivePortalPage":
        Services.obs.notifyObservers(null, "ensure-captive-portal-tab");
        break;
      case "Browser:PrimeMitm":
        this.primeMitm(this.browser);
        break;
      case "Browser:ResetEnterpriseRootsPref":
        Services.prefs.clearUserPref("security.enterprise_roots.enabled");
        Services.prefs.clearUserPref("security.enterprise_roots.auto-enabled");
        break;
      case "Browser:ResetSSLPreferences":
        let prefSSLImpact = PREF_SSL_IMPACT_ROOTS.reduce((prefs, root) => {
          return prefs.concat(Services.prefs.getChildList(root));
        }, []);
        for (let prefName of prefSSLImpact) {
          Services.prefs.clearUserPref(prefName);
        }
        this.browser.reload();
        break;
      case "Browser:SSLErrorGoBack":
        this.goBackFromErrorPage(this.browser.ownerGlobal);
        break;
      case "Browser:SSLErrorReportTelemetry":
        let reportStatus = message.data.reportStatus;
        Services.telemetry
          .getHistogramById("TLS_ERROR_REPORT_UI")
          .add(reportStatus);
        break;
      case "GetChangedCertPrefs":
        let hasChangedCertPrefs = this.hasChangedCertPrefs();
        this.sendAsyncMessage("HasChangedCertPrefs", {
          hasChangedCertPrefs,
        });
        break;
      case "ReportBlockingError":
        this.ReportBlockingError(
          this.browsingContext.id,
          message.data.scheme,
          message.data.host,
          message.data.port,
          message.data.path,
          message.data.xfoAndCspInfo
        );
        break;
      case "DisplayOfflineSupportPage":
        this.displayOfflineSupportPage(message.data.supportPageSlug);
        break;
      case "Browser:CertExceptionError":
        switch (message.data.elementId) {
          case "viewCertificate": {
            let window = this.browser.ownerGlobal;

            let securityInfo = this.getSecurityInfo(
              message.data.securityInfoAsString
            );
            let certChain = securityInfo.failedCertChain;
            let certs = certChain.map(elem =>
              encodeURIComponent(elem.getBase64DERString())
            );
            let certsStringURL = certs.map(elem => `cert=${elem}`);
            certsStringURL = certsStringURL.join("&");
            let url = `about:certificate?${certsStringURL}`;
            window.switchToTabHavingURI(url, true, {});
            break;
          }
        }
    }
  }
}
