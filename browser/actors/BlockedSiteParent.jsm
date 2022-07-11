/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["BlockedSiteParent"];

class BlockedSiteParent extends JSWindowActorParent {
  receiveMessage(msg) {
    switch (msg.name) {
      case "Browser:SiteBlockedError":
        this._onAboutBlocked(
          msg.data.elementId,
          msg.data.reason,
          this.browsingContext === this.browsingContext.top,
          msg.data.blockedInfo
        );
        break;
    }
  }

  _onAboutBlocked(elementId, reason, isTopFrame, blockedInfo) {
    let browser = this.browsingContext.top.embedderElement;
    if (!browser) {
      return;
    }
    let { BrowserOnClick } = browser.ownerGlobal;
    // Depending on what page we are displaying here (malware/phishing/unwanted)
    // use the right strings and links for each.
    let bucketName = "";
    let sendTelemetry = false;
    if (reason === "malware") {
      sendTelemetry = true;
      bucketName = "WARNING_MALWARE_PAGE_";
    } else if (reason === "phishing") {
      sendTelemetry = true;
      bucketName = "WARNING_PHISHING_PAGE_";
    } else if (reason === "unwanted") {
      sendTelemetry = true;
      bucketName = "WARNING_UNWANTED_PAGE_";
    } else if (reason === "harmful") {
      sendTelemetry = true;
      bucketName = "WARNING_HARMFUL_PAGE_";
    }
    let secHistogram = Services.telemetry.getHistogramById(
      "URLCLASSIFIER_UI_EVENTS"
    );
    let nsISecTel = Ci.IUrlClassifierUITelemetry;
    bucketName += isTopFrame ? "TOP_" : "FRAME_";

    switch (elementId) {
      case "goBackButton":
        if (sendTelemetry) {
          secHistogram.add(nsISecTel[bucketName + "GET_ME_OUT_OF_HERE"]);
        }
        browser.ownerGlobal.getMeOutOfHere(this.browsingContext);
        break;
      case "ignore_warning_link":
        if (Services.prefs.getBoolPref("browser.safebrowsing.allowOverride")) {
          if (sendTelemetry) {
            secHistogram.add(nsISecTel[bucketName + "IGNORE_WARNING"]);
          }
          BrowserOnClick.ignoreWarningLink(
            reason,
            blockedInfo,
            this.browsingContext
          );
        }
        break;
    }
  }
}
