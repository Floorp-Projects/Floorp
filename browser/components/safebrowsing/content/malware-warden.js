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
# The Original Code is Google Safe Browsing.
#
# The Initial Developer of the Original Code is Google Inc.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Dave Camp <dcamp@mozilla.com>
#   Fritz Schneider <fritz@google.com> (original phishing-warden.js author)
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

// This warden manages updates to the malware list

const kMalwareWardenEnabledPref = "browser.safebrowsing.malware.enabled";

function PROT_MalwareWarden() {
  PROT_ListWarden.call(this);

  this.debugZone = "malwarewarden";

  // Use this to query preferences
  this.prefs_ = new G_Preferences();

  // Global preference to enable the malware warden
  this.malwareWardenEnabled_ =
    this.prefs_.getPref(kMalwareWardenEnabledPref, null);

  // Get notifications when the malware warden enabled pref changes
  var malwareWardenPrefObserver =
    BindToObject(this.onMalwareWardenEnabledPrefChanged, this);
  this.prefs_.addObserver(kMalwareWardenEnabledPref, malwareWardenPrefObserver);

  // Add a test chunk to the database
  var testData = "mozilla.org/firefox/its-an-attack.html";

  var testUpdate =
    "n:1000\ni:test-malware-simple\nad:1\n" +
    "a:1:32:" + testData.length + "\n" +
    testData;
    
  testData = "mozilla.org/firefox/its-a-trap.html";
  testUpdate +=
    "n:1000\ni:test-phish-simple\nad:1\n" +
    "a:1:32:" + testData.length + "\n" +
    testData;

  var dbService_ = Cc["@mozilla.org/url-classifier/dbservice;1"]
                   .getService(Ci.nsIUrlClassifierDBService);

  var listener = {
    QueryInterface: function(iid)
    {
      if (iid.equals(Ci.nsISupports) ||
          iid.equals(Ci.nsIUrlClassifierUpdateObserver))
        return this;
      throw Cr.NS_ERROR_NO_INTERFACE;
    },

    updateUrlRequested: function(url) { },
    streamFinished: function(status) { },
    updateError: function(errorCode) { },
    updateSuccess: function(requestedTimeout) { }
  };

  try {
    dbService_.beginUpdate(listener,
                           "test-malware-simple,test-phish-simple", "");
    dbService_.beginStream("", "");
    dbService_.updateStream(testUpdate);
    dbService_.finishStream();
    dbService_.finishUpdate();
  } catch(ex) {
    // beginUpdate will throw harmlessly if there's an existing update
    // in progress, ignore failures.
  }
  G_Debug(this, "malwareWarden initialized");
}

PROT_MalwareWarden.inherits(PROT_ListWarden);

/**
 * Cleanup on shutdown.
 */
PROT_MalwareWarden.prototype.shutdown = function() {
  this.prefs_.removeAllObservers();

  this.listManager_ = null;
}

/**
 * When a preference changes, we might have to start or stop asking for
 * updates.
 */
PROT_MalwareWarden.prototype.maybeToggleUpdateChecking = function() {
  var malwareWardenEnabled = this.prefs_.getPref(kMalwareWardenEnabledPref,
                                                 null);

  G_Debug(this, "Maybe toggling update checking. " +
          "Warden enabled? " + malwareWardenEnabled);

  // Do nothing unless thre pref is set
  if (malwareWardenEnabled === null)
    return;

  // We update and save to disk all tables
  if (malwareWardenEnabled === true) {
    this.enableBlacklistTableUpdates();
  } else {
    // Anti-malware is off, disable table updates
    this.disableBlacklistTableUpdates();
  }
}

/**
 * Deal with a user changing the pref that says whether we should 
 * enable the malware warden.
 *
 * @param prefName Name of the pref holding the value indicating whether
 *                 we should enable the malware warden
 */
PROT_MalwareWarden.prototype.onMalwareWardenEnabledPrefChanged = function(
                                                                    prefName) {
  // Just to be safe, ignore changes to sub prefs.
  if (prefName != kMalwareWardenEnabledPref)
    return;

  this.malwareWardenEnabled_ =
    this.prefs_.getPref(prefName, this.malwareWardenEnabled_);
  this.maybeToggleUpdateChecking();
}
