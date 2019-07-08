/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AboutNetErrorHandler"];

const { RemotePages } = ChromeUtils.import(
  "resource://gre/modules/remotepagemanager/RemotePageManagerParent.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm"
);

var AboutNetErrorHandler = {
  _inited: false,
  _topics: [
    "Browser:OpenCaptivePortalPage",
    "Browser:PrimeMitm",
    "Browser:ResetEnterpriseRootsPref",
    "RecordCertErrorLoad",
  ],

  init() {
    this._boundReceiveMessage = this.receiveMessage.bind(this);
    this.pageListener = new RemotePages(["about:certerror", "about:neterror"]);
    for (let topic of this._topics) {
      this.pageListener.addMessageListener(topic, this._boundReceiveMessage);
    }
    this._inited = true;

    Services.obs.addObserver(this, "captive-portal-login-abort");
    Services.obs.addObserver(this, "captive-portal-login-success");
  },

  uninit() {
    if (!this._inited) {
      return;
    }

    for (let topic of this._topics) {
      this.pageListener.removeMessageListener(topic, this._boundReceiveMessage);
    }
    this.pageListener.destroy();

    Services.obs.removeObserver(this, "captive-portal-login-abort");
    Services.obs.removeObserver(this, "captive-portal-login-success");
  },

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "captive-portal-login-abort":
      case "captive-portal-login-success":
        // Send a message to the content when a captive portal is freed
        // so that error pages can refresh themselves.
        this.pageListener.sendAsyncMessage("AboutNetErrorCaptivePortalFreed");
        break;
    }
  },

  receiveMessage(msg) {
    switch (msg.name) {
      case "Browser:OpenCaptivePortalPage":
        Services.obs.notifyObservers(null, "ensure-captive-portal-tab");
        break;
      case "Browser:PrimeMitm":
        this.primeMitm(msg.target.browser);
        break;
      case "Browser:ResetEnterpriseRootsPref":
        Services.prefs.clearUserPref("security.enterprise_roots.enabled");
        Services.prefs.clearUserPref("security.enterprise_roots.auto-enabled");
        break;
      case "RecordCertErrorLoad":
        Services.telemetry.recordEvent(
          "security.ui.certerror",
          "load",
          "aboutcerterror",
          msg.data.errorCode,
          {
            has_sts: msg.data.has_sts.toString(),
            is_frame: msg.data.is_frame.toString(),
          }
        );
        break;
    }
  },

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
  },
};
