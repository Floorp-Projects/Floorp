/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Via web-panels.xul
/* import-globals-from browser.js */

const NS_ERROR_MODULE_NETWORK = 2152398848;
const NS_NET_STATUS_READ_FROM = NS_ERROR_MODULE_NETWORK + 8;
const NS_NET_STATUS_WROTE_TO  = NS_ERROR_MODULE_NETWORK + 9;

function getPanelBrowser() {
    return document.getElementById("web-panels-browser");
}

var panelProgressListener = {
    onProgressChange(aWebProgress, aRequest,
                                aCurSelfProgress, aMaxSelfProgress,
                                aCurTotalProgress, aMaxTotalProgress) {
    },

    onStateChange(aWebProgress, aRequest, aStateFlags, aStatus) {
        if (!aRequest)
          return;

        // ignore local/resource:/chrome: files
        if (aStatus == NS_NET_STATUS_READ_FROM || aStatus == NS_NET_STATUS_WROTE_TO)
           return;

        if (aStateFlags & Ci.nsIWebProgressListener.STATE_START &&
            aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) {
            window.parent.document.getElementById("sidebar-throbber").setAttribute("loading", "true");
        } else if (aStateFlags & Ci.nsIWebProgressListener.STATE_STOP &&
                aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) {
            window.parent.document.getElementById("sidebar-throbber").removeAttribute("loading");
        }
    },

    onLocationChange(aWebProgress, aRequest, aLocation, aFlags) {
        UpdateBackForwardCommands(getPanelBrowser().webNavigation);
    },

    onStatusChange(aWebProgress, aRequest, aStatus, aMessage) {
    },

    onSecurityChange(aWebProgress, aRequest, aState) {
    },

    QueryInterface(aIID) {
        if (aIID.equals(Ci.nsIWebProgressListener) ||
            aIID.equals(Ci.nsISupportsWeakReference) ||
            aIID.equals(Ci.nsISupports))
            return this;
        throw Cr.NS_NOINTERFACE;
    }
};

var gLoadFired = false;
function loadWebPanel(aURI) {
    var panelBrowser = getPanelBrowser();
    if (gLoadFired) {
        panelBrowser.webNavigation
                    .loadURI(aURI, nsIWebNavigation.LOAD_FLAGS_NONE,
                             null, null, null);
    }
    panelBrowser.setAttribute("cachedurl", aURI);
}

function load() {
    var panelBrowser = getPanelBrowser();
    panelBrowser.webProgress.addProgressListener(panelProgressListener,
                                                 Ci.nsIWebProgress.NOTIFY_ALL);
    panelBrowser.messageManager.loadFrameScript("chrome://browser/content/content.js", true);
    var cachedurl = panelBrowser.getAttribute("cachedurl");
    if (cachedurl) {
        panelBrowser.webNavigation
                    .loadURI(cachedurl, nsIWebNavigation.LOAD_FLAGS_NONE, null,
                             null, null);
    }

    gLoadFired = true;
}

function unload() {
    getPanelBrowser().webProgress.removeProgressListener(panelProgressListener);
}

function PanelBrowserStop() {
    getPanelBrowser().webNavigation.stop(nsIWebNavigation.STOP_ALL);
}

function PanelBrowserReload() {
    getPanelBrowser().webNavigation
                     .sessionHistory
                     .QueryInterface(nsIWebNavigation)
                     .reload(nsIWebNavigation.LOAD_FLAGS_NONE);
}
