# -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# ***** BEGIN LICENSE BLOCK *****
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
# The Original Code is Firefox Anti-Phishing Support.
#
# The Initial Developer of the Original Code is
# Mozilla Corporation.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Jeff Walden <jwalden+code@mit.edu>   (original author)
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

/**
 * gPhishDialog controls the user interface for displaying the privacy policy of
 * an anti-phishing provider. 
 * 
 * The caller (gSecurityPane._userAgreedToPhishingEULA in main.js) invokes this
 * dialog with a single argument - a reference to an object with .providerNum
 * and .userAgreed properties.  This code displays the dialog for the provider
 * as dictated by .providerNum and loads the policy.  When that load finishes,
 * the OK button is enabled and the user can either accept or decline the
 * agreement (a choice which is communicated by setting .userAgreed to true if
 * the user did indeed agree).
 */ 
var gPhishDialog = {
  /**
   * The nsIWebProgress object associated with the privacy policy frame.
   */
  _webProgress: null,

  /**
   * Initializes UI and starts the privacy policy loading.
   */
  init: function ()
  {
    const Cc = Components.classes, Ci = Components.interfaces;

    var providerNum = window.arguments[0].providerNum;

    var phishBefore = document.getElementById("phishBefore");

    var prefb = Cc["@mozilla.org/preferences-service;1"].
                getService(Ci.nsIPrefService).
                getBranch("browser.safebrowsing.provider.");

    // init before-frame and after-frame strings
    // note that description only wraps when the string is the element's
    // *content* and *not* when it's the value attribute
    var providerName = prefb.getComplexValue(providerNum + ".name", Ci.nsISupportsString).data
    var strings = document.getElementById("bundle_phish");
    phishBefore.textContent = strings.getFormattedString("phishBeforeText", [providerName]);

    // guaranteed to be present, because only providers with privacy policies
    // are displayed in the prefwindow
    var formatter = Cc["@mozilla.org/toolkit/URLFormatterService;1"].
                    getService(Ci.nsIURLFormatter);
    var privacyURL = formatter.formatURLPref("browser.safebrowsing.provider." +
                                             providerNum +
                                             ".privacy.url",
                                             null);
    var fallbackURL = formatter.formatURLPref("browser.safebrowsing.provider." +
                                              providerNum +
                                              ".privacy.fallbackurl",
                                              null);
    this._progressListener._providerFallbackURL = fallbackURL;

    // add progress listener to enable OK, radios when page loads
    var frame = document.getElementById("phishPolicyFrame");
    var docShell = frame.docShell;

    // disable unnecessary features
    docShell.allowAuth = false;
    docShell.allowPlugins = false;
    docShell.allowSubframes = false;

    var webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.addProgressListener(this._progressListener,
                                    Ci.nsIWebProgress.NOTIFY_STATE_WINDOW);

    this._webProgress = webProgress; // for easy use later

    // start loading the privacyURL
    const loadFlags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
    frame.webNavigation.loadURI(privacyURL, loadFlags, null, null, null);
  },

  /**
   * The nsIWebProgressListener used to watch the status of the load of the
   * privacy policy; enables the OK button when the load completes.
   */
  _progressListener:
  {
    /**
     * First we try to load the provider url (possibly remote). If that fails
     * to load, we try to load the provider fallback url (must be chrome://).
     * If that also fails, we display an error message.
     */
    _providerLoadFailed: false,
    _providerFallbackLoadFailed: false,
    
    _tryLoad: function(url) {
      const Ci = Components.interfaces;
      const loadFlags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
      var frame = document.getElementById("phishPolicyFrame");
      frame.webNavigation.loadURI(url, loadFlags, null, null, null);
    },

    onStateChange: function (aWebProgress, aRequest, aStateFlags, aStatus)
    {
      // enable the OK button when the request completes
      const Ci = Components.interfaces, Cr = Components.results;
      if ((aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) &&
          (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW)) {
        
        if (Components.isSuccessCode(aRequest.status)) {
          try {
            aRequest.QueryInterface(Ci.nsIHttpChannel);
          } catch (e) {
            // Not an http request url (might be, e.g., chrome:) that loaded
            // successfully, so we can exit.
            return;
          }

          // Any response other than 200 OK is an error.
          if (200 == aRequest.responseStatus)
            return;
        }

        // Something failed
        if (!this._providerLoadFailed) {
          this._provderLoadFailed = true;
          // Remote EULA failed to load; try loading provider fallback
          this._tryLoad(this._providerFallbackURL);
        } else if (!this._providerFallbackLoadFailed) {
          // Provider fallback failed to load; try loading fallback EULA
          this._providerFallbackLoadFailed = true;

          // fire off a load of the fallback policy
          const fallbackURL = "chrome://browser/content/preferences/fallbackEULA.xhtml";
          this._tryLoad(fallbackURL);

          // disable radios
          document.getElementById("acceptOrDecline").disabled = true;
        } else {
          throw "Fallback policy failed to load -- what the hay!?!";
        }
      }
    },
    
    onProgressChange: function(aWebProgress, aRequest, aCurSelfProgress,
                               aMaxSelfProgress, aCurTotalProgress,
                               aMaxTotalProgress)
    {
    },

    onStatusChange : function(aWebProgress, aRequest, aStatus, aMessage)
    {
    },

    QueryInterface : function(aIID)
    {
      const Ci = Components.interfaces;
      if (aIID.equals(Ci.nsIWebProgressListener) ||
          aIID.equals(Ci.nsISupportsWeakReference) ||
          aIID.equals(Ci.nsISupports))
        return this;
      throw Components.results.NS_NOINTERFACE;
    }
  },

  /**
   * Signals that the user accepted the privacy policy by setting the window
   * arguments appropriately.  Note that this does *not* change preferences;
   * the opener of this dialog handles that.
   */
  accept: function ()
  {
    window.arguments[0].userAgreed = true;
  },

  /**
   * Clean up any XPCOM-JS cycles we may have created.
   */
  uninit: function ()
  {
    // overly aggressive, but better safe than sorry
    this._webProgress.removeProgressListener(this._progressListener);
    this._progressListener = this._webProgress = null;
  },

  /**
   * Called when the user changes the agree/disagree radio.
   */
  onchangeRadio: function ()
  {
    var radio = document.getElementById("acceptOrDecline");
    document.documentElement.getButton("accept").disabled = (radio.value == "false");
  }
};
