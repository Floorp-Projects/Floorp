# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
