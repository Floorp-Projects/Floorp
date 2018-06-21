/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["BlockedSiteContent"];

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

      blockedInfo = { list: classifiedChannel.matchedList,
                      provider: classifiedChannel.matchedProvider,
                      uri: reportUri.asciiSpec };
    }
  }
  return blockedInfo;
}

var BlockedSiteContent = {
  receiveMessage(global, msg) {
    if (msg.name == "DeceptiveBlockedDetails") {
      global.sendAsyncMessage("DeceptiveBlockedDetails:Result", {
        blockedInfo: getSiteBlockedErrorDetails(global.docShell),
      });
    }
  },

  handleEvent(global, aEvent) {
    if (aEvent.type != "AboutBlockedLoaded") {
      return;
    }

    let {content} = global;

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
      doc.getElementById("error_desc_link").setAttribute("href", desc + aEvent.detail.url);
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
    if (!advisoryUrl) {
      let el = content.document.getElementById("advisoryDesc");
      el.remove();
      return;
    }

    let advisoryLinkText = Services.prefs.getCharPref(
      "browser.safebrowsing.provider." + provider + ".advisoryName", "");
    if (!advisoryLinkText) {
      let el = content.document.getElementById("advisoryDesc");
      el.remove();
      return;
    }

    let anchorEl = content.document.getElementById("advisory_provider");
    anchorEl.setAttribute("href", advisoryUrl);
    anchorEl.textContent = advisoryLinkText;
  },

  onAboutBlocked(global, targetElement, ownerDoc) {
    var reason = "phishing";
    if (/e=malwareBlocked/.test(ownerDoc.documentURI)) {
      reason = "malware";
    } else if (/e=unwantedBlocked/.test(ownerDoc.documentURI)) {
      reason = "unwanted";
    } else if (/e=harmfulBlocked/.test(ownerDoc.documentURI)) {
      reason = "harmful";
    }

    let docShell = ownerDoc.defaultView.QueryInterface(Ci.nsIInterfaceRequestor)
                                       .getInterface(Ci.nsIWebNavigation)
                                      .QueryInterface(Ci.nsIDocShell);

    global.sendAsyncMessage("Browser:SiteBlockedError", {
      location: ownerDoc.location.href,
      reason,
      elementId: targetElement.getAttribute("id"),
      isTopFrame: (ownerDoc.defaultView.parent === ownerDoc.defaultView),
      blockedInfo: getSiteBlockedErrorDetails(docShell),
    });
  },
};
