/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["NetErrorChild"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { RemotePageChild } = ChromeUtils.import(
  "resource://gre/actors/RemotePageChild.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gSerializationHelper",
  "@mozilla.org/network/serialization-helper;1",
  "nsISerializationHelper"
);

class NetErrorChild extends RemotePageChild {
  actorCreated() {
    super.actorCreated();

    const exportableFunctions = [
      "RPMGetAppBuildID",
      "RPMPrefIsLocked",
      "RPMAddToHistogram",
    ];
    this.exportFunctions(exportableFunctions);
  }

  getSerializedSecurityInfo(docShell) {
    let securityInfo =
      docShell.failedChannel && docShell.failedChannel.securityInfo;
    if (!securityInfo) {
      return "";
    }
    securityInfo
      .QueryInterface(Ci.nsITransportSecurityInfo)
      .QueryInterface(Ci.nsISerializable);

    return gSerializationHelper.serializeToString(securityInfo);
  }

  handleEvent(aEvent) {
    // Documents have a null ownerDocument.
    let doc = aEvent.originalTarget.ownerDocument || aEvent.originalTarget;

    switch (aEvent.type) {
      case "click":
        let elem = aEvent.originalTarget;
        if (elem.id == "viewCertificate") {
          // Call through the superclass to avoid the security check.
          this.sendAsyncMessage("Browser:CertExceptionError", {
            location: doc.location.href,
            elementId: elem.id,
            securityInfoAsString: this.getSerializedSecurityInfo(
              doc.defaultView.docShell
            ),
          });
        }
        break;
    }
  }

  RPMGetAppBuildID() {
    return Services.appinfo.appBuildID;
  }

  RPMPrefIsLocked(aPref) {
    return Services.prefs.prefIsLocked(aPref);
  }

  RPMAddToHistogram(histID, bin) {
    Services.telemetry.getHistogramById(histID).add(bin);
  }
}
