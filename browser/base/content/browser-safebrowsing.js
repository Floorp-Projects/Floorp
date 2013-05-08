# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifdef MOZ_SAFE_BROWSING
var gSafeBrowsing = {

  setReportPhishingMenu: function() {

    // A phishing page will have a specific about:blocked content documentURI
    var isPhishingPage = content.document.documentURI.startsWith("about:blocked?e=phishingBlocked");

    // Show/hide the appropriate menu item.
    document.getElementById("menu_HelpPopup_reportPhishingtoolmenu")
            .hidden = isPhishingPage;
    document.getElementById("menu_HelpPopup_reportPhishingErrortoolmenu")
            .hidden = !isPhishingPage;

    var broadcasterId = isPhishingPage
                        ? "reportPhishingErrorBroadcaster"
                        : "reportPhishingBroadcaster";

    var broadcaster = document.getElementById(broadcasterId);
    if (!broadcaster)
      return;

    var uri = getBrowser().currentURI;
    if (uri && (uri.schemeIs("http") || uri.schemeIs("https")))
      broadcaster.removeAttribute("disabled");
    else
      broadcaster.setAttribute("disabled", true);
  },

  /**
   * Used to report a phishing page or a false positive
   * @param name String One of "Phish", "Error", "Malware" or "MalwareError"
   * @return String the report phishing URL.
   */
  getReportURL: function(name) {
    var reportUrl = SafeBrowsing.getReportURL(name);

    var pageUri = gBrowser.currentURI.clone();

    // Remove the query to avoid including potentially sensitive data
    if (pageUri instanceof Ci.nsIURL)
      pageUri.query = '';

    reportUrl += "&url=" + encodeURIComponent(pageUri.asciiSpec);

    return reportUrl;
  }
}
#endif
