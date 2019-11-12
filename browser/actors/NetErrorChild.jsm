/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["NetErrorChild"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { ChildMessagePort } = ChromeUtils.import(
  "resource://gre/modules/remotepagemanager/RemotePageManagerChild.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gSerializationHelper",
  "@mozilla.org/network/serialization-helper;1",
  "nsISerializationHelper"
);

class NetErrorChild extends JSWindowActorChild {
  actorCreated() {
    this.messagePort = new ChildMessagePort(this, this.contentWindow);
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

  receiveMessage(aMessage) {
    this.messagePort.handleMessage(aMessage);
  }

  handleEvent(aEvent) {
    // Documents have a null ownerDocument.
    let doc = aEvent.originalTarget.ownerDocument || aEvent.originalTarget;

    switch (aEvent.type) {
      case "click":
        let elem = aEvent.originalTarget;
        if (elem.id == "viewCertificate") {
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
}
