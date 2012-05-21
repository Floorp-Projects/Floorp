# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

var safebrowsing = {
  startup: function() {
    setTimeout(function() {
      safebrowsing.deferredStartup();
    }, 2000);
    window.removeEventListener("load", safebrowsing.startup, false);
  },

  deferredStartup: function() {
    this.appContext.initialize();
  },

  setReportPhishingMenu: function() {
      
    // A phishing page will have a specific about:blocked content documentURI
    var isPhishingPage = /^about:blocked\?e=phishingBlocked/.test(content.document.documentURI);
    
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
   * Lazy init getter for appContext
   */
  get appContext() {
    delete this.appContext;
    return this.appContext = Cc["@mozilla.org/safebrowsing/application;1"]
                            .getService().wrappedJSObject;
  },

  /**
   * Used to report a phishing page or a false positive
   * @param name String One of "Phish", "Error", "Malware" or "MalwareError"
   * @return String the report phishing URL.
   */
  getReportURL: function(name) {
    var reportUrl = this.appContext.getReportURL(name);

    var pageUrl = getBrowser().currentURI.asciiSpec;
    reportUrl += "&url=" + encodeURIComponent(pageUrl);

    return reportUrl;
  }
}

window.addEventListener("load", safebrowsing.startup, false);
