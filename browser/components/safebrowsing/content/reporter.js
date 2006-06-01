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


// A tiny class to do reporting for us. We report interesting user actions
// such as the user hitting a blacklisted page, and the user accepting
// or declining the warning.
//
// Each report has a subject and data. Current reports are:
//
// subject         data     meaning
// --------------------------------
// phishnavaway    url      the user navigated away from a phishy page
// phishdecline    url      the user declined our warning
// phishaccept     url      the user accepted our warning
// phishblhit      url      the user loaded a phishing page
//
// We only send reports in advanced protection mode, and even then we
// strip cookies from the request before sending it.

/**
 * A very complicated class to send pings to the provider. The class does
 * nothing if we're not in advanced protection mode.
 *
 * @constructor
 */
function PROT_Reporter() {
  this.debugZone = "reporter";
  this.prefs_ = new G_Preferences();
}

/**
 * Send a report!
 *
 * @param subject String indicating what this report is about (will be 
 *                urlencoded)
 * @param data String giving extra information about this report (will be 
 *                urlencoded)
 */
PROT_Reporter.prototype.report = function(subject, data) {
  // Send a report iff we're in advanced protection mode
  if (!this.prefs_.getPref(kPhishWardenRemoteLookups, false))
    return;
  // Make sure a report url is defined
  var url = null;
  try {
   url = PROT_GlobalStore.getActionReportURL();
  } catch (e) {
  }
  if (!url)
    return;

  url += "evts=" + encodeURIComponent(subject)
         + "&evtd=" + encodeURIComponent(data);
  G_Debug(this, "Sending report: " + url);
  (new PROT_XMLFetcher(true /* strip cookies */)).get(url, null /* no cb */);
}
