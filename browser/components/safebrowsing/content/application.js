# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

// We instantiate this variable when we create the application.
var gDataProvider = null;

// An instance of our application is a PROT_Application object. It
// basically just populates a few globals and instantiates wardens,
// the listmanager, and the about:blocked error page.

/**
 * An instance of our application. There should be exactly one of these.
 * 
 * Note: This object should instantiated only at profile-after-change
 * or later because the listmanager and the cryptokeymanager need to
 * read and write data files. Additionally, NSS isn't loaded until
 * some time around then (Moz bug #321024).
 *
 * @constructor
 */
function PROT_Application() {
  this.debugZone= "application";

#ifdef DEBUG
  // TODO This is truly lame; we definitely want something better
  function runUnittests() {
    if (false) {

      G_DebugL("UNITTESTS", "STARTING UNITTESTS");
      TEST_G_Protocol4Parser();
      TEST_G_CryptoHasher();
      TEST_PROT_EnchashDecrypter();
      TEST_PROT_TRTable();
      TEST_PROT_ListManager();
      TEST_PROT_PhishingWarden();
      TEST_PROT_TRFetcher();
      TEST_PROT_URLCanonicalizer();
      TEST_G_Preferences();
      TEST_G_Observer();
      TEST_PROT_WireFormat();
      // UrlCrypto's test should come before the key manager's
      TEST_PROT_UrlCrypto();
      TEST_PROT_UrlCryptoKeyManager();
      G_DebugL("UNITTESTS", "END UNITTESTS");
    }
  };

  runUnittests();
#endif
  
  // expose some classes
  this.PROT_PhishingWarden = PROT_PhishingWarden;
  this.PROT_MalwareWarden = PROT_MalwareWarden;

  // Load data provider pref values
  gDataProvider = new PROT_DataProvider();

  // expose the object
  this.wrappedJSObject = this;
}

var gInitialized = false;
PROT_Application.prototype.initialize = function() {
  if (gInitialized)
    return;
  gInitialized = true;

  var obs = Cc["@mozilla.org/observer-service;1"].
            getService(Ci.nsIObserverService);
  obs.addObserver(this, "xpcom-shutdown", true);

  // XXX: move table names to a pref that we originally will download
  // from the provider (need to workout protocol details)
  this.malwareWarden = new PROT_MalwareWarden();
  this.malwareWarden.registerBlackTable("goog-malware-shavar");
  this.malwareWarden.maybeToggleUpdateChecking();

  this.phishWarden = new PROT_PhishingWarden();
  this.phishWarden.registerBlackTable("goog-phish-shavar");
  this.phishWarden.maybeToggleUpdateChecking();
}

PROT_Application.prototype.observe = function(subject, topic, data) {
  switch (topic) {
    case "xpcom-shutdown":
      this.malwareWarden.shutdown();
      this.phishWarden.shutdown();
      break;
  }
}

/**
 * @param name String The type of url to get (either Phish or Error).
 * @return String the report phishing URL (localized).
 */
PROT_Application.prototype.getReportURL = function(name) {
  return gDataProvider["getReport" + name + "URL"]();
}

PROT_Application.prototype.QueryInterface = function(iid) {
  if (iid.equals(Ci.nsISupports) ||
      iid.equals(Ci.nsISupportsWeakReference) ||
      iid.equals(Ci.nsIObserver))
    return this;

  throw Components.results.NS_ERROR_NO_INTERFACE;
}
