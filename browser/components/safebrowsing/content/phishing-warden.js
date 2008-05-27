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
#   Fritz Schneider <fritz@google.com> (original author)
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


// The warden checks request to see if they are for phishy pages. It
// does so by querying our locally stored blacklists.
// 
// When the warden notices a problem, it queries all browser views
// (each of which corresopnds to an open browser window) to see
// whether one of them can handle it. A browser view can handle a
// problem if its browser window has an HTMLDocument loaded with the
// given URL and that Document hasn't already been flagged as a
// problem. For every problematic URL we notice loading, at most one
// Document is flagged as problematic. Otherwise you can get into
// trouble if multiple concurrent phishy pages load with the same URL.
//
// Since we check URLs very early in the request cycle (in a progress
// listener), the URL might not yet be associated with a Document when
// we determine that it is phishy. So the the warden retries finding
// a browser view to handle the problem until one can, or until it
// determines it should give up (see complicated logic below).
//
// The warden has displayers that the browser view uses to render
// different kinds of warnings (e.g., one that's shown before a page
// loads as opposed to one that's shown after the page has already
// loaded).
//
// Note: There is a single warden for the whole application.
//
// TODO better way to expose displayers/views to browser view

const kPhishWardenEnabledPref = "browser.safebrowsing.enabled";

/**
 * Abtracts the checking of user/browser actions for signs of
 * phishing. 
 *
 * @param progressListener nsIDocNavStartProgressListener
 * @param tabbrowser XUL tabbrowser element
 * @constructor
 */
function PROT_PhishingWarden() {
  PROT_ListWarden.call(this);

  this.debugZone = "phishwarden";

  // Use this to query preferences
  this.prefs_ = new G_Preferences();

  // We need to know whether we're enabled and whether we're in advanced
  // mode, so reflect the appropriate preferences into our state.

  // Global preference to enable the phishing warden
  this.phishWardenEnabled_ = this.prefs_.getPref(kPhishWardenEnabledPref, null);

  // Get notifications when the phishing warden enabled pref changes
  var phishWardenPrefObserver = 
    BindToObject(this.onPhishWardenEnabledPrefChanged, this);
  this.prefs_.addObserver(kPhishWardenEnabledPref, phishWardenPrefObserver);

  G_Debug(this, "phishWarden initialized");
}

PROT_PhishingWarden.inherits(PROT_ListWarden);

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
  this.prefs_.removeAllObservers();
  this.listManager_ = null;
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
  var phishWardenEnabled = this.prefs_.getPref(kPhishWardenEnabledPref, null);

  G_Debug(this, "Maybe toggling update checking. " +
          "Warden enabled? " + phishWardenEnabled);

  // Do nothing unless both prefs are set.  They can be null (unset), true, or
  // false.
  if (phishWardenEnabled === null)
    return;

  // We update and save to disk all tables
  if (phishWardenEnabled === true) {
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
  // Just to be safe, ignore changes to sub prefs.
  if (prefName != "browser.safebrowsing.enabled")
    return;

  this.phishWardenEnabled_ = 
    this.prefs_.getPref(prefName, this.phishWardenEnabled_);
  this.maybeToggleUpdateChecking();
}
