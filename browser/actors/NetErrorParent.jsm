/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["NetErrorParent"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gSerializationHelper",
  "@mozilla.org/network/serialization-helper;1",
  "nsISerializationHelper"
);

class NetErrorParent extends JSWindowActorParent {
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

  receiveMessage(message) {
    switch (message.name) {
      case "Browser:CertExceptionError":
        switch (message.data.elementId) {
          case "viewCertificate": {
            let browser = this.browsingContext.top.embedderElement;
            if (!browser) {
              break;
            }

            let window = browser.ownerGlobal;

            let securityInfo = this.getSecurityInfo(message.data.securityInfoAsString);
            let cert = securityInfo.serverCert;
            if (Services.prefs.getBoolPref("security.aboutcertificate.enabled")) {
              let certChain = securityInfo.failedCertChain;
              let certs = certChain.map(elem =>
                encodeURIComponent(elem.getBase64DERString())
              );
              let certsStringURL = certs.map(elem => `cert=${elem}`);
              certsStringURL = certsStringURL.join("&");
              let url = `about:certificate?${certsStringURL}`;
              if (window.openTrustedLinkIn) {
                window.openTrustedLinkIn(url, "tab");
              }
            } else {
              Services.ww.openWindow(
                window,
                "chrome://pippki/content/certViewer.xul",
                "_blank",
                "centerscreen,chrome",
                cert
              );
            }
            break;
          }
        }
    }
  }
}
