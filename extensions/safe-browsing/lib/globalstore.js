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
// TODO: this should probably be part of the Application

/**
 * A clearinghouse for globals. All interfaces are read-only.
 */
function PROT_GlobalStore(prefix) {
  this.debugZone = "globalstore";
  this.prefix_ = prefix;
}

/**
 * @returns The name of the pref determining whether phishing protection
 *          is enabled (i.e., whether SafeBrowsing is enabled)
 */
PROT_GlobalStore.prototype.getPhishWardenEnabledPrefName = function() {
  return this.prefix_ + "enabled";
}

/**
 * @returns The name of the pref determining whether we enable remote
 *          checking (advanced protection)
 */
PROT_GlobalStore.prototype.getServerCheckEnabledPrefName = function() {
  return this.prefix_ + "advancedprotection";
}

/**
 * @returns The name of the pref determining whether we send reports 
 *          about user actions
 */
PROT_GlobalStore.prototype.getSendUserReportsPrefName = function() {
  // We send reports iff we're in advanced protection mode
  return this.getServerCheckEnabledPrefName();
}
 
/**
 * @returns The name of the pref controlling whether we skip local
 *          whitelist checking. [Deprecated]
 */
PROT_GlobalStore.prototype.getSkipLocalWhitelistPrefName = function() {
  return this.prefix_ + "skip.local-whitelist";
}

/**
 * @returns The name of the directory in which we should store data
 */
PROT_GlobalStore.prototype.getAppDirectoryName = function() {
  return "google_firefox_safebrowsing";
}

/**
 * @returns String containing the URL to nav to when the user clicks
 *          "get me out of here"
 */
PROT_GlobalStore.prototype.getGetMeOutOfHereURL = function() {

  // Try to get their homepage, but fall back on Google
  var url = "http://www.google.com/";

  var prefs = Cc["@mozilla.org/preferences-service;1"]
              .getService(Ci.nsIPrefService).getBranch(null);

  try {
    var url = prefs.getComplexValue("browser.startup.homepage", 
                                    Ci.nsIPrefLocalizedString).data;
  } catch(e) {
    G_Debug(this, "Couldn't get homepage pref: " + e);
  }
  
  return url;
}

/**
 * @returns String containing the URL to nav to when the user clicks
 *          the link to antiphishing.org in the bubble.
 */
PROT_GlobalStore.prototype.getAntiPhishingURL = function() {
  return "http://antiphishing.org"; 
}

/**
 * @returns String containing the URL to nav to when the user clicks
 *          on the policy link in the preferences.
 */
PROT_GlobalStore.prototype.getPolicyURL = function() {
  return "http://www.google.com/privacy.html";
}

/**
 * @returns String containing the URL to nav to when the user wants to 
 *          submit a generic phishing report (we're not sure if they 
 *          want to report a false positive or negative).
 */
PROT_GlobalStore.prototype.getGenericPhishSubmitURL = function() {
  return "http://www.google.com/safebrowsing/report_general/?continue=http%3A%2F%2Fwww.google.com%2Ftools%2Ffirefox%2Fsafebrowsing%2Fsubmit_success.html";
}

/**
 * @returns String containing the URL to nav to when the user wants to 
 *          report a false positive (i.e. a non-phishy page)
 */
PROT_GlobalStore.prototype.getFalsePositiveURL = function() {
  return "http://www.google.com/safebrowsing/report_error/?continue=http%3A%2F%2Fwww.google.com%2Ftools%2Ffirefox%2Fsafebrowsing%2Fsubmit_success.html";
}

/**
 * @returns String containing the URL to nav to when the user wants to 
 *          report a false negative (i.e. a phishy page)
 */
PROT_GlobalStore.prototype.getSubmitUrl = function() {
  return "http://www.google.com/safebrowsing/report_phish/?continue=http%3A%2F%2Fwww.google.com%2Ftools%2Ffirefox%2Fsafebrowsing%2Fsubmit_success.html";
}

/**
 * @returns String containing the URL to nav to when the user clicks
 *          "more info" in the bubble or the product link in the preferences.
 */
PROT_GlobalStore.prototype.getHomePageURL = function() {
  return "http://www.google.com/tools/service/npredir?r=ff_sb_home";
}

/**
 * @returns String containing the URL to nav to when the user clicks
 *          "phishing FAQ" in the bubble.
 */
PROT_GlobalStore.prototype.getPhishingFaqURL = function() {
  return "http://www.google.com/tools/service/npredir?r=ff_sb_faq#phishing";
}

/**
 * @returns String containing the URL to nav to when the user wants to 
 *          see the test page 
 */
PROT_GlobalStore.prototype.getTestURL = function() {
  return "http://www.google.com/tools/firefox/safebrowsing/phish-o-rama.html";
}

/**
 * @returns String giving url to use for lookups
 */
PROT_GlobalStore.prototype.getLookupserverURL = function() {
  return "http://www.google.com/safebrowsing/lookup?";
}

/**
 * @returns String giving url to use for updates
 */
PROT_GlobalStore.prototype.getUpdateserverURL = function() {
  return "http://www.google.com/safebrowsing/update?";
}

/**
 * @returns String giving url to use to report actions
 */
PROT_GlobalStore.prototype.getActionReportURL = function() {
  return "http://www.google.com/safebrowsing/report?";
}

/**
 * @returns String giving url to use for re-keying
 */
PROT_GlobalStore.prototype.getGetKeyURL = function() {
  return "https://www.google.com/safebrowsing/getkey?";
}

/**
 * @returns String giving filename to use to store keys
 */
PROT_GlobalStore.prototype.getKeyFilename = function() {
  return "kf.txt";
}

/**
 * @returns String giving the name of the extension 
 *          (as per install.rdf)
 */
PROT_GlobalStore.prototype.getExtensionName = function() {
  return "Google Safe Browsing";
}



