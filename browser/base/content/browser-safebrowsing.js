/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

var gSafeBrowsing = {

  setReportPhishingMenu() {
    // In order to detect whether or not we're at the phishing warning
    // page, we have to check the documentURI instead of the currentURI.
    // This is because when the DocShell loads an error page, the
    // currentURI stays at the original target, while the documentURI
    // will point to the internal error page we loaded instead.
    var docURI = gBrowser.selectedBrowser.documentURI;
    var isPhishingPage =
      docURI && docURI.spec.startsWith("about:blocked?e=deceptiveBlocked");

    // Show/hide the appropriate menu item.
    document.getElementById("menu_HelpPopup_reportPhishingtoolmenu")
            .hidden = isPhishingPage;
    document.getElementById("menu_HelpPopup_reportPhishingErrortoolmenu")
            .hidden = !isPhishingPage;

    // Now look at the currentURI to learn which page we were trying
    // to browse to.
    const uri = gBrowser.currentURI;
    const isReportablePage = uri && (uri.schemeIs("http") || uri.schemeIs("https"));

    const disabledByPolicy = !Services.policies.isAllowed("feedbackCommands");

    const reportBroadcaster = document.getElementById("reportPhishingBroadcaster");
    if (disabledByPolicy || isPhishingPage || !isReportablePage) {
      reportBroadcaster.setAttribute("disabled", "true");
    } else {
      reportBroadcaster.removeAttribute("disabled");
    }

    const reportErrorBroadcaster = document.getElementById("reportPhishingErrorBroadcaster");
    if (disabledByPolicy || !isPhishingPage || !isReportablePage) {
      reportErrorBroadcaster.setAttribute("disabled", "true");
    } else {
      reportErrorBroadcaster.removeAttribute("disabled");
    }
  },

  /**
   * Used to report a phishing page or a false positive
   *
   * @param name
   *        String One of "PhishMistake", "MalwareMistake", or "Phish"
   * @param info
   *        Information about the reasons for blocking the resource.
   *        In the case false positive, it may contain SafeBrowsing
   *        matching list and provider of the list
   * @return String the report phishing URL.
   */
  getReportURL(name, info) {
    let reportInfo = info;
    if (!reportInfo) {
      let pageUri = gBrowser.currentURI;

      // Remove the query to avoid including potentially sensitive data
      if (pageUri instanceof Ci.nsIURL) {
        pageUri = pageUri.mutate()
                         .setQuery("")
                         .finalize();
      }

      reportInfo = { uri: pageUri.asciiSpec };
    }
    return SafeBrowsing.getReportURL(name, reportInfo);
  }
};
