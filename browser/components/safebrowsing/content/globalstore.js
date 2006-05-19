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


// A class that encapsulates globals such as the names of things.  We
// centralize everything here mainly so as to ease their modification,
// but also in case we want to version.
//
// This class does _not_ embody semantics, defaults, or the like. If we
// need something that does, we'll add our own preference registry.
//
// TODO: many of these values should just be moved directly into code.
// TODO: The code needs to fail more gracefully if these values aren't set
//       E.g., createInstance should fail for listmanager without these.

/**
 * A clearinghouse for globals. All interfaces are read-only.
 */
function PROT_GlobalStore() {
}

/**
 * Read a pref value
 */
PROT_GlobalStore.getPref_ = function(prefname) {
  var pref = new G_Preferences();
  return pref.getPref(prefname);
}

/**
 * @returns The name of the pref determining whether phishing protection
 *          is enabled (i.e., whether SafeBrowsing is enabled)
 */
PROT_GlobalStore.getPhishWardenEnabledPrefName = function() {
  return "browser.safebrowsing.enabled";
}

/**
 * @returns The name of the pref determining whether we enable remote
 *          checking (advanced protection)
 */
PROT_GlobalStore.getServerCheckEnabledPrefName = function() {
  return "browser.safebrowsing.remoteLookups";
}

/**
 * @returns The name of the pref determining whether we send reports 
 *          about user actions
 */
PROT_GlobalStore.getSendUserReportsPrefName = function() {
  // We send reports iff advanced protection mode is on
  return PROT_GlobalStore.getServerCheckEnabledPrefName();
}

/**
 * @returns The name of the directory in which we should store data (like
 *          blacklists and whitelists). This is relative to the user's
 *          profile.
 */
PROT_GlobalStore.getAppDirectoryName = function() {
  return "safebrowsing_data";
}

/**
 * @returns String containing the URL to nav to when the user clicks
 *          "get me out of here"
 */
PROT_GlobalStore.getGetMeOutOfHereURL = function() {
  // Try to get their homepage from prefs.
  var prefs = Cc["@mozilla.org/preferences-service;1"]
              .getService(Ci.nsIPrefService).getBranch(null);

  var url = "about:blank";
  try {
    url = prefs.getComplexValue("browser.startup.homepage",
                                Ci.nsIPrefLocalizedString).data;
  } catch(e) {
    G_Debug(this, "Couldn't get homepage pref: " + e);
  }
  
  return url;
}

/**
 * TODO: maybe deprecate because antiphishing.org isn't localized
 * @returns String containing the URL to nav to when the user clicks
 *          the link to antiphishing.org in the bubble.
 */
PROT_GlobalStore.getAntiPhishingURL = function() {
  return "http://antiphishing.org/"; 
}

/**
 * @returns String containing the URL to nav to when the user clicks
 *          on the policy link in the preferences.
 */
PROT_GlobalStore.getPolicyURL = function() {
  return "TODO";
}

/**
 * @returns String containing the URL to nav to when the user wants to 
 *          submit a generic phishing report (we're not sure if they 
 *          want to report a false positive or negative).
 */
PROT_GlobalStore.getGenericPhishSubmitURL = function() {
  return PROT_GlobalStore.getPref_("browser.safebrowsing.provider.0.genericReportURL");
}

/**
 * @returns String containing the URL to nav to when the user wants to 
 *          report a false positive (i.e. a non-phishy page)
 */
PROT_GlobalStore.getFalsePositiveURL = function() {
  return PROT_GlobalStore.getPref_("browser.safebrowsing.provider.0.reportErrorURL");
}

/**
 * @returns String containing the URL to nav to when the user wants to 
 *          report a false negative (i.e. a phishy page)
 */
PROT_GlobalStore.getSubmitUrl = function() {
  return PROT_GlobalStore.getPref_("browser.safebrowsing.provider.0.reportPhishURL");
}

/**
 * TODO: maybe deprecated because no UI location for it?
 * @returns String containing the URL to nav to when the user clicks
 *          "more info" in the bubble or the product link in the preferences.
 */
PROT_GlobalStore.getHomePageURL = function() {
  return PROT_GlobalStore.getPref_("browser.safebrowsing.provider.0.homeURL");
}

/**
 * TODO: maybe deprecated because no UI location for it?
 * @returns String containing the URL to nav to when the user clicks
 *          "phishing FAQ" in the bubble.
 */
PROT_GlobalStore.getPhishingFaqURL = function() {
  return PROT_GlobalStore.getPref_("browser.safebrowsing.provider.0.faqURL");
}

/**
 * @returns String containing the URL to nav to when the user wants to 
 *          see the test page 
 */
PROT_GlobalStore.getTestURLs = function() {
  // TODO: return all test urls
  return [PROT_GlobalStore.getPref_("browser.safebrowsing.provider.0.testURL")];
}

/**
 * @returns String giving url to use for lookups (used in advanced mode)
 */
PROT_GlobalStore.getLookupserverURL = function() {
  return PROT_GlobalStore.getPref_("browser.safebrowsing.provider.0.lookupURL");
}

/**
 * @returns String giving url to use for updates (diff of lists)
 */
PROT_GlobalStore.getUpdateserverURL = function() {
  return PROT_GlobalStore.getPref_("browser.safebrowsing.provider.0.updateURL");
}

/**
 * TODO: maybe deprecate?
 * @returns String giving url to use to report actions (advanced mode only
 */
PROT_GlobalStore.getActionReportURL = function() {
  return PROT_GlobalStore.getPref_("browser.safebrowsing.provider.0.reportURL");
}

/**
 * @returns String giving url to use for re-keying
 */
PROT_GlobalStore.getGetKeyURL = function() {
  return PROT_GlobalStore.getPref_("browser.safebrowsing.provider.0.keyURL");
}
