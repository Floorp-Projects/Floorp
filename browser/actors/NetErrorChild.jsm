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

const lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gSerializationHelper",
  "@mozilla.org/network/serialization-helper;1",
  "nsISerializationHelper"
);

class NetErrorChild extends RemotePageChild {
  actorCreated() {
    super.actorCreated();

    // If you add a new function, remember to add it to RemotePageAccessManager.jsm
    // to allow content-privileged about:neterror or about:certerror to use it.
    const exportableFunctions = [
      "RPMGetAppBuildID",
      "RPMGetInnerMostURI",
      "RPMAddToHistogram",
      "RPMRecordTelemetryEvent",
      "RPMGetHttpResponseHeader",
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

    return lazy.gSerializationHelper.serializeToString(securityInfo);
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

  RPMGetInnerMostURI(uriString) {
    let uri = Services.io.newURI(uriString);
    if (uri instanceof Ci.nsINestedURI) {
      uri = uri.QueryInterface(Ci.nsINestedURI).innermostURI;
    }

    return uri.spec;
  }

  RPMGetAppBuildID() {
    return Services.appinfo.appBuildID;
  }

  RPMAddToHistogram(histID, bin) {
    Services.telemetry.getHistogramById(histID).add(bin);
  }

  RPMRecordTelemetryEvent(category, event, object, value, extra) {
    Services.telemetry.recordEvent(category, event, object, value, extra);
  }

  // Get the header from the http response of the failed channel. This function
  // is used in the 'about:neterror' page.
  RPMGetHttpResponseHeader(responseHeader) {
    let channel = this.contentWindow.docShell.failedChannel;
    if (!channel) {
      return "";
    }

    let httpChannel = channel.QueryInterface(Ci.nsIHttpChannel);
    if (!httpChannel) {
      return "";
    }

    try {
      return httpChannel.getResponseHeader(responseHeader);
    } catch (e) {}

    return "";
  }
}
