/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
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
 * The Original Code is Places Unit Test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Marco Bonardo <mak77@bonardo.net>
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

/**
 * Tests that requesting clear history at shutdown will really clear history.
 */

const URIS = [
  "http://a.example1.com/"
, "http://b.example1.com/"
, "http://b.example2.com/"
, "http://c.example3.com/"
];

let expirationObserver = {
  observe: function observe(aSubject, aTopic, aData) {
    print("Finished expiration.");
    Services.obs.removeObserver(expirationObserver,
                                PlacesUtils.TOPIC_EXPIRATION_FINISHED);
  
    let db = PlacesUtils.history
                        .QueryInterface(Ci.nsPIPlacesDatabase)
                        .DBConnection;

    let stmt = db.createStatement(
      "SELECT id FROM moz_places_temp WHERE url = :page_url "
    + "UNION ALL "
    + "SELECT id FROM moz_places WHERE url = :page_url "
    );

    try {
      URIS.forEach(function(aUrl) {
        stmt.params.page_url = aUrl;
        do_check_false(stmt.executeStep());
        stmt.reset();
      });
    } finally {
      stmt.finalize();
    }

    do_test_finished();
  }
}

let timeInMicroseconds = Date.now() * 1000;

function run_test() {
  do_test_pending();

  print("Initialize browserglue before Places");
  Cc["@mozilla.org/browser/browserglue;1"].getService(Ci.nsIBrowserGlue);

  Services.prefs.setBoolPref("privacy.clearOnShutdown.cache", true);
  Services.prefs.setBoolPref("privacy.clearOnShutdown.cookies", true);
  Services.prefs.setBoolPref("privacy.clearOnShutdown.offlineApps", true);
  Services.prefs.setBoolPref("privacy.clearOnShutdown.history", true);
  Services.prefs.setBoolPref("privacy.clearOnShutdown.downloads", true);
  Services.prefs.setBoolPref("privacy.clearOnShutdown.cookies", true);
  Services.prefs.setBoolPref("privacy.clearOnShutdown.formData", true);
  Services.prefs.setBoolPref("privacy.clearOnShutdown.passwords", true);
  Services.prefs.setBoolPref("privacy.clearOnShutdown.sessions", true);
  Services.prefs.setBoolPref("privacy.clearOnShutdown.siteSettings", true);

  Services.prefs.setBoolPref("privacy.sanitize.sanitizeOnShutdown", true);

  print("Add visits.");
  URIS.forEach(function(aUrl) {
    PlacesUtils.history.addVisit(uri(aUrl), timeInMicroseconds++, null,
                                 PlacesUtils.history.TRANSITION_TYPED,
                                 false, 0);
  });

  print("Wait expiration.");
  Services.obs.addObserver(expirationObserver,
                           PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);
  print("Simulate shutdown.");
  PlacesUtils.history.QueryInterface(Ci.nsIObserver)
                     .observe(null, TOPIC_GLOBAL_SHUTDOWN, null);
}
