# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is Mozilla.org Code.
# 
# The Initial Developer of the Original Code is David Hyatt.
# Portions created by the Initial Developer are Copyright (C) 2001
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
#   David Hyatt <hyatt@mozilla.org>
# 
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK *****

var panelProgressListener = {
    onProgressChange : function (aWebProgress, aRequest,
                                    aCurSelfProgress, aMaxSelfProgress,
                                    aCurTotalProgress, aMaxTotalProgress) {
    },
    
    onStateChange : function(aWebProgress, aRequest, aStateFlags, aStatus)
    {
        if (!aRequest)
          return;

        //ignore local/resource:/chrome: files
        if (aStatus == NS_NET_STATUS_READ_FROM || aStatus == NS_NET_STATUS_WROTE_TO)
           return;

        const nsIWebProgressListener = Components.interfaces.nsIWebProgressListener;
        const nsIChannel = Components.interfaces.nsIChannel;
        if (aStateFlags & nsIWebProgressListener.STATE_START && 
            aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK) {
            window.parent.document.getElementById('sidebar-throbber').setAttribute("loading", "true");
        }
        else if (aStateFlags & nsIWebProgressListener.STATE_STOP &&
                aStateFlags & nsIWebProgressListener.STATE_IS_NETWORK) {
            window.parent.document.getElementById('sidebar-throbber').removeAttribute("loading");
        }
    }
    ,

    onLocationChange : function(aWebProgress, aRequest, aLocation) {
    },

    onStatusChange : function(aWebProgress, aRequest, aStatus, aMessage) {
    },

    onSecurityChange : function(aWebProgress, aRequest, aState) { 
    },

    QueryInterface : function(aIID)
    {
        if (aIID.equals(Components.interfaces.nsIWebProgressListener) ||
            aIID.equals(Components.interfaces.nsISupportsWeakReference) ||
            aIID.equals(Components.interfaces.nsISupports))
        return this;
        throw Components.results.NS_NOINTERFACE;
    }
};

var gLoadFired = false;
function loadWebPanel(aURI) {
    var panelBrowser = document.getElementById('web-panels-browser');
    if (gLoadFired)
        panelBrowser.webNavigation.loadURI(aURI, nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);
    panelBrowser.setAttribute("cachedurl", aURI);
}

function load()
{
  var panelBrowser = document.getElementById('web-panels-browser');
  panelBrowser.webProgress.addProgressListener(panelProgressListener, Components.interfaces.nsIWebProgress.NOTIFY_ALL);
  if (panelBrowser.getAttribute("cachedurl"))
    panelBrowser.webNavigation.loadURI(panelBrowser.getAttribute("cachedurl"), nsIWebNavigation.LOAD_FLAGS_NONE, null, null, null);
  gNavigatorBundle = document.getElementById("bundle_browser");
    
  gLoadFired = true;
}

function unload()
{
  var panelBrowser = document.getElementById('web-panels-browser');
  panelBrowser.webProgress.removeProgressListener(panelProgressListener);
}
