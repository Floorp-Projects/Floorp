/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["BlockedSiteChild"];

const {ActorChild} = ChromeUtils.import("resource://gre/modules/ActorChild.jsm");
ChromeUtils.defineModuleGetter(this, "E10SUtils",
  "resource://gre/modules/E10SUtils.jsm");

ChromeUtils.defineModuleGetter(this, "SafeBrowsing",
                               "resource://gre/modules/SafeBrowsing.jsm");

function getSiteBlockedErrorDetails(docShell) {
  let blockedInfo = {};
  if (docShell.failedChannel) {
    let classifiedChannel = docShell.failedChannel.
                            QueryInterface(Ci.nsIClassifiedChannel);
    if (classifiedChannel) {
      let httpChannel = docShell.failedChannel.QueryInterface(Ci.nsIHttpChannel);

      let reportUri = httpChannel.URI;

      // Remove the query to avoid leaking sensitive data
      if (reportUri instanceof Ci.nsIURL) {
        reportUri = reportUri.mutate()
                             .setQuery("")
                             .finalize();
      }

      let triggeringPrincipal = docShell.failedChannel.loadInfo ? E10SUtils.serializePrincipal(docShell.failedChannel.loadInfo.triggeringPrincipal) : null;
      blockedInfo = { list: classifiedChannel.matchedList,
                      triggeringPrincipal,
                      provider: classifiedChannel.matchedProvider,
                      uri: reportUri.asciiSpec };
    }
  }
  return blockedInfo;
}

class BlockedSiteChild extends ActorChild {
  receiveMessage(msg) {
    if (msg.name == "DeceptiveBlockedDetails") {
      this.mm.sendAsyncMessage("DeceptiveBlockedDetails:Result", {
        blockedInfo: getSiteBlockedErrorDetails(this.mm.docShell),
      });
    }
  }

  handleEvent(event) {
    if (event.type == "AboutBlockedLoaded") {
      this.onAboutBlockedLoaded(event);
    } else if (event.type == "click" && event.button == 0) {
      this.onClick(event);
    }
  }

  onAboutBlockedLoaded(aEvent) {
    let global = this.mm;
    let content = aEvent.target.ownerGlobal;

    let blockedInfo = getSiteBlockedErrorDetails(global.docShell);
    let provider = blockedInfo.provider || "";

    let doc = content.document;

    /**
    * Set error description link in error details.
    * For example, the "reported as a deceptive site" link for
    * blocked phishing pages.
    */
    let desc = Services.prefs.getCharPref(
      "browser.safebrowsing.provider." + provider + ".reportURL", "");
    if (desc) {
      doc.getElementById("error_desc_link").setAttribute("href", desc + encodeURIComponent(aEvent.detail.url));
    }

    // Set other links in error details.
    switch (aEvent.detail.err) {
      case "malware":
        doc.getElementById("report_detection").setAttribute("href",
          (SafeBrowsing.getReportURL("MalwareMistake", blockedInfo) ||
           "https://www.stopbadware.org/firefox"));
        doc.getElementById("learn_more_link").setAttribute("href",
          "https://www.stopbadware.org/firefox");
        break;
      case "unwanted":
        doc.getElementById("learn_more_link").setAttribute("href",
          "https://www.google.com/about/unwanted-software-policy.html");
        break;
      case "phishing":
        doc.getElementById("report_detection").setAttribute("href",
          (SafeBrowsing.getReportURL("PhishMistake", blockedInfo) ||
           "https://safebrowsing.google.com/safebrowsing/report_error/?tpl=mozilla"));
        doc.getElementById("learn_more_link").setAttribute("href",
          "https://www.antiphishing.org//");
        break;
    }

    // Set the firefox support url.
    doc.getElementById("firefox_support").setAttribute("href",
      Services.urlFormatter.formatURLPref("app.support.baseURL") + "phishing-malware");

    // Show safe browsing details on load if the pref is set to true.
    let showDetails = Services.prefs.getBoolPref("browser.xul.error_pages.show_safe_browsing_details_on_load");
    if (showDetails) {
      let details = content.document.getElementById("errorDescriptionContainer");
      details.removeAttribute("hidden");
    }

    // Set safe browsing advisory link.
    let advisoryUrl = Services.prefs.getCharPref(
      "browser.safebrowsing.provider." + provider + ".advisoryURL", "");
    let advisoryDesc = content.document.getElementById("advisoryDescText");
    if (!advisoryUrl) {
      advisoryDesc.remove();
      return;
    }

    let advisoryLinkText = Services.prefs.getCharPref(
      "browser.safebrowsing.provider." + provider + ".advisoryName", "");
    if (!advisoryLinkText) {
      advisoryDesc.remove();
      return;
    }

    content.document.l10n.setAttributes(advisoryDesc, "safeb-palm-advisory-desc", {"advisoryname": advisoryLinkText});
    content.document.getElementById("advisory_provider").setAttribute("href", advisoryUrl);
  }

  onClick(event) {
    let ownerDoc = event.target.ownerDocument;
    if (!ownerDoc) {
      return;
    }

    var reason = "phishing";
    if (/e=malwareBlocked/.test(ownerDoc.documentURI)) {
      reason = "malware";
    } else if (/e=unwantedBlocked/.test(ownerDoc.documentURI)) {
      reason = "unwanted";
    } else if (/e=harmfulBlocked/.test(ownerDoc.documentURI)) {
      reason = "harmful";
    }

    this.mm.sendAsyncMessage("Browser:SiteBlockedError", {
      location: ownerDoc.location.href,
      reason,
      elementId: event.target.getAttribute("id"),
      isTopFrame: (ownerDoc.defaultView.parent === ownerDoc.defaultView),
      blockedInfo: getSiteBlockedErrorDetails(ownerDoc.defaultView.docShell),
    });
  }
}
