/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["NetErrorChild"];

const { ActorChild } = ChromeUtils.import(
  "resource://gre/modules/ActorChild.jsm"
);

function getSerializedSecurityInfo(docShell) {
  let serhelper = Cc["@mozilla.org/network/serialization-helper;1"].getService(
    Ci.nsISerializationHelper
  );

  let securityInfo =
    docShell.failedChannel && docShell.failedChannel.securityInfo;
  if (!securityInfo) {
    return "";
  }
  securityInfo
    .QueryInterface(Ci.nsITransportSecurityInfo)
    .QueryInterface(Ci.nsISerializable);

  return serhelper.serializeToString(securityInfo);
}

class NetErrorChild extends ActorChild {
  isAboutNetError(doc) {
    return doc.documentURI.startsWith("about:neterror");
  }

  handleEvent(aEvent) {
    // Documents have a null ownerDocument.
    let doc = aEvent.originalTarget.ownerDocument || aEvent.originalTarget;

    switch (aEvent.type) {
      case "AboutNetErrorSetAutomatic":
        this.onSetAutomatic(aEvent);
        break;
      case "AboutNetErrorResetPreferences":
        this.onResetPreferences(aEvent);
        break;
      case "click":
        let elem = aEvent.originalTarget;
        if (
          elem.id == "viewCertificate" ||
          elem.id == "exceptionDialogButton"
        ) {
          this.mm.sendAsyncMessage("Browser:CertExceptionError", {
            location: doc.location.href,
            elementId: elem.id,
            securityInfoAsString: getSerializedSecurityInfo(
              doc.defaultView.docShell
            ),
          });
        }
        break;
    }
  }

  onResetPreferences(evt) {
    this.mm.sendAsyncMessage("Browser:ResetSSLPreferences");
  }

  onSetAutomatic(evt) {
    this.mm.sendAsyncMessage("Browser:SetSSLErrorReportAuto", {
      automatic: evt.detail,
    });

    // If we're enabling reports, send a report for this failure.
    if (evt.detail) {
      let win = evt.originalTarget.ownerGlobal;
      let docShell = win.docShell;

      let { securityInfo } = docShell.failedChannel;
      securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
      let { host, port } = win.document.mozDocumentURIIfNotForErrorPages;

      let errorReporter = Cc["@mozilla.org/securityreporter;1"].getService(
        Ci.nsISecurityReporter
      );
      errorReporter.reportTLSError(securityInfo, host, port);
    }
  }
}
