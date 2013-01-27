/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that requesting clear history at shutdown will really clear history.
 */

const URIS = [
  "http://a.example1.com/"
, "http://b.example1.com/"
, "http://b.example2.com/"
, "http://c.example3.com/"
];

const TOPIC_CONNECTION_CLOSED = "places-connection-closed";

let EXPECTED_NOTIFICATIONS = [
  "places-shutdown"
, "places-will-close-connection"
, "places-expiration-finished"
, "places-connection-closed"
];

const UNEXPECTED_NOTIFICATIONS = [
  "xpcom-shutdown"
];

const URL = "ftp://localhost/clearHistoryOnShutdown/";

let notificationIndex = 0;

let notificationsObserver = {
  observe: function observe(aSubject, aTopic, aData) {
    print("Received notification: " + aTopic);

    // Note that some of these notifications could arrive multiple times, for
    // example in case of sync, we allow that.
    if (EXPECTED_NOTIFICATIONS[notificationIndex] != aTopic)
      notificationIndex++;
    do_check_eq(EXPECTED_NOTIFICATIONS[notificationIndex], aTopic);

    if (aTopic != TOPIC_CONNECTION_CLOSED)
      return;

    getDistinctNotifications().forEach(
      function (topic) Services.obs.removeObserver(notificationsObserver, topic)
    );

    print("Looking for uncleared stuff.");

    let stmt = DBConn().createStatement(
      "SELECT id FROM moz_places WHERE url = :page_url "
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

    // Check cache.
    checkCache(URL);
  }
}

let timeInMicroseconds = Date.now() * 1000;

function run_test() {
  run_next_test();
}

add_task(function test_execute() {
  do_test_pending();

  print("Initialize browserglue before Places");

  // Avoid default bookmarks import.
  let glue = Cc["@mozilla.org/browser/browserglue;1"].
             getService(Ci.nsIObserver);
  glue.observe(null, "initial-migration-will-import-default-bookmarks", null);

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
  for (let aUrl of URIS) {
    yield promiseAddVisits({uri: uri(aUrl), visitDate: timeInMicroseconds++,
                            transition: PlacesUtils.history.TRANSITION_TYPED})
  }
  print("Add cache.");
  storeCache(URL, "testData");
});

function run_test_continue()
{
  print("Simulate and wait shutdown.");
  getDistinctNotifications().forEach(
    function (topic)
      Services.obs.addObserver(notificationsObserver, topic, false)
  );

  shutdownPlaces();

  // Shutdown the download manager.
  Services.obs.notifyObservers(null, "quit-application", null);
}

function getDistinctNotifications() {
  let ar = EXPECTED_NOTIFICATIONS.concat(UNEXPECTED_NOTIFICATIONS);
  return [ar[i] for (i in ar) if (ar.slice(0, i).indexOf(ar[i]) == -1)];
}

function storeCache(aURL, aContent) {
  let cache = Cc["@mozilla.org/network/cache-service;1"].
              getService(Ci.nsICacheService);
  let session = cache.createSession("FTP", Ci.nsICache.STORE_ANYWHERE,
                                    Ci.nsICache.STREAM_BASED);

  var storeCacheListener = {
    onCacheEntryAvailable: function (entry, access, status) {
      do_check_eq(status, Cr.NS_OK);

      entry.setMetaDataElement("servertype", "0");
      var os = entry.openOutputStream(0);

      var written = os.write(aContent, aContent.length);
      if (written != aContent.length) {
        do_throw("os.write has not written all data!\n" +
                 "  Expected: " + written  + "\n" +
                 "  Actual: " + aContent.length + "\n");
      }
      os.close();
      entry.close();
      do_execute_soon(run_test_continue);
    }
  };

  session.asyncOpenCacheEntry(aURL,
                              Ci.nsICache.ACCESS_READ_WRITE,
                              storeCacheListener);
}


function checkCache(aURL) {
  let cache = Cc["@mozilla.org/network/cache-service;1"].
              getService(Ci.nsICacheService);
  let session = cache.createSession("FTP", Ci.nsICache.STORE_ANYWHERE,
                                    Ci.nsICache.STREAM_BASED);

  var checkCacheListener = {
    onCacheEntryAvailable: function (entry, access, status) {
      do_check_eq(status, Cr.NS_ERROR_CACHE_KEY_NOT_FOUND);
      do_test_finished();
    }
  };

  session.asyncOpenCacheEntry(aURL,
                              Ci.nsICache.ACCESS_READ,
                              checkCacheListener);
}
