/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fritz Schneider <fritz@google.com> (original author)
 *   Scott MacGregor <mscott@mozilla.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


// The warden checks URLs to see if they are phishing URLs. It
// does so by querying our locally stored blacklists (privacy
// mode).
//
// Note: There is a single warden for the whole application.

const kPhishWardenEnabledPref = "browser.safebrowsing.enabled";

// We have hardcoded URLs that let people navigate to in order to 
// check out the warning.
const kTestUrls = {
  "http://www.google.com/tools/firefox/safebrowsing/phish-o-rama.html": true,
  "http://www.mozilla.org/projects/bonecho/anti-phishing/its-a-trap.html": true,
  "http://www.mozilla.com/firefox/its-a-trap.html": true,
}

/**
 * Abtracts the checking of user/browser actions for signs of
 * phishing. 
 *
 * @param progressListener nsIDocNavStartProgressListener
 * @constructor
 */
function PROT_PhishingWarden(progressListener) {
  PROT_ListWarden.call(this);

  this.debugZone = "phishwarden";
  this.testing_ = false;

  // Use this to query preferences
  this.prefs_ = new G_Preferences();
  
  // Global preference to enable the phishing warden
  this.phishWardenEnabled_ = this.prefs_.getPref(kPhishWardenEnabledPref, null);

  // Get notifications when the phishing warden enabled pref changes
  var phishWardenPrefObserver = 
    BindToObject(this.onPhishWardenEnabledPrefChanged, this);
  this.prefs_.addObserver(kPhishWardenEnabledPref, phishWardenPrefObserver);
  
  // Get notifications when the data provider pref changes
  var dataProviderPrefObserver =
    BindToObject(this.onDataProviderPrefChanged, this);
  this.prefs_.addObserver(kDataProviderIdPref, dataProviderPrefObserver);

  G_Debug(this, "phishWarden initialized");
}

PROT_PhishingWarden.inherits(PROT_ListWarden);

/**
 * We implement nsIWebProgressListener
 */
PROT_PhishingWarden.prototype.QueryInterface = function(iid) {
  if (iid.equals(Ci.nsISupports) || 
      iid.equals(Ci.nsISupportsWeakReference))
    return this;
  throw Components.results.NS_ERROR_NO_INTERFACE;
}

/**
 * Cleanup on shutdown.
 */
PROT_PhishingWarden.prototype.shutdown = function() {
}

/**
 * When a preference (either advanced features or the phishwarden
 * enabled) changes, we might have to start or stop asking for updates. 
 * 
 * This is a little tricky; we start or stop management only when we
 * have complete information we can use to determine whether we
 * should.  It could be the case that one pref or the other isn't set
 * yet (e.g., they haven't opted in/out of advanced features). So do
 * nothing unless we have both pref values -- we get notifications for
 * both, so eventually we will start correctly.
 */ 
PROT_PhishingWarden.prototype.maybeToggleUpdateChecking = function() {
  if (this.testing_)
    return;

  var phishWardenEnabled = this.prefs_.getPref(kPhishWardenEnabledPref, null);

  // Do nothing unless the phishing warden pref is set.  It can be null (unset), true, or
  // false.
  if (phishWardenEnabled === null)
    return;

  // We update and save to disk all tables if we don't have remote checking
  // enabled.
  if (phishWardenEnabled === true) {
    // If anti-phishing is enabled, we always download the local files to
    // use in case remote lookups fail.
    this.enableBlacklistTableUpdates();
    this.enableWhitelistTableUpdates();
  } else {
    // Anti-phishing is off, disable table updates
    this.disableBlacklistTableUpdates();
    this.disableWhitelistTableUpdates();
  }
}

/**
 * Deal with a user changing the pref that says whether we should 
 * enable the phishing warden (i.e., that SafeBrowsing is active)
 *
 * @param prefName Name of the pref holding the value indicating whether
 *                 we should enable the phishing warden
 */
PROT_PhishingWarden.prototype.onPhishWardenEnabledPrefChanged = function(
                                                                    prefName) {
  this.phishWardenEnabled_ = this.prefs_.getBoolPrefOrDefault(prefName, this.phishWardenEnabled_);
  this.maybeToggleUpdateChecking();
}

/**
 * Event fired when the user changes data providers.
 */
PROT_PhishingWarden.prototype.onDataProviderPrefChanged = function(prefName) {
}

/**
 * Indicates if this URL is one of the possible blacklist test URLs.
 * These test URLs should always be considered as phishy.
 *
 * @param url URL to check 
 * @return A boolean indicating whether this is one of our blacklist
 *         test URLs
 */
PROT_PhishingWarden.prototype.isBlacklistTestURL = function(url) {
  // Explicitly check for URL so we don't get JS warnings in strict mode.
  if (kTestUrls[url])
    return true;
  return false;
}

/**
 * Callback for found local blacklist match.  First we report that we have
 * a blacklist hit, then we bring up the warning dialog.
 * @param status Number enum from callback (PROT_ListWarden.IN_BLACKLIST,
 *    PROT_ListWarden.IN_WHITELIST, PROT_ListWarden.NOT_FOUND)
 */
PROT_PhishingWarden.prototype.localListMatch_ = function(url, request, status) {
  if (PROT_ListWarden.IN_BLACKLIST != status)
    return;
}
