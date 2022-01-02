/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tests that requesting clear history at shutdown will really clear history.
 */

const URIS = [
  "http://a.example1.com/",
  "http://b.example1.com/",
  "http://b.example2.com/",
  "http://c.example3.com/",
];

const FTP_URL = "ftp://localhost/clearHistoryOnShutdown/";

const { Sanitizer } = ChromeUtils.import("resource:///modules/Sanitizer.jsm");

// Send the profile-after-change notification to the form history component to ensure
// that it has been initialized.
var formHistoryStartup = Cc[
  "@mozilla.org/satchel/form-history-startup;1"
].getService(Ci.nsIObserver);
formHistoryStartup.observe(null, "profile-after-change", null);
ChromeUtils.defineModuleGetter(
  this,
  "FormHistory",
  "resource://gre/modules/FormHistory.jsm"
);

var timeInMicroseconds = Date.now() * 1000;

add_task(async function test_execute() {
  info("Initialize browserglue before Places");

  // Avoid default bookmarks import.
  let glue = Cc["@mozilla.org/browser/browserglue;1"].getService(
    Ci.nsIObserver
  );
  glue.observe(null, "initial-migration-will-import-default-bookmarks", null);
  Sanitizer.onStartup();

  Services.prefs.setBoolPref(Sanitizer.PREF_SHUTDOWN_BRANCH + "cache", true);
  Services.prefs.setBoolPref(Sanitizer.PREF_SHUTDOWN_BRANCH + "cookies", true);
  Services.prefs.setBoolPref(
    Sanitizer.PREF_SHUTDOWN_BRANCH + "offlineApps",
    true
  );
  Services.prefs.setBoolPref(Sanitizer.PREF_SHUTDOWN_BRANCH + "history", true);
  Services.prefs.setBoolPref(
    Sanitizer.PREF_SHUTDOWN_BRANCH + "downloads",
    true
  );
  Services.prefs.setBoolPref(Sanitizer.PREF_SHUTDOWN_BRANCH + "cookies", true);
  Services.prefs.setBoolPref(Sanitizer.PREF_SHUTDOWN_BRANCH + "formData", true);
  Services.prefs.setBoolPref(Sanitizer.PREF_SHUTDOWN_BRANCH + "sessions", true);
  Services.prefs.setBoolPref(
    Sanitizer.PREF_SHUTDOWN_BRANCH + "siteSettings",
    true
  );

  Services.prefs.setBoolPref(Sanitizer.PREF_SANITIZE_ON_SHUTDOWN, true);

  info("Add visits.");
  for (let aUrl of URIS) {
    await PlacesTestUtils.addVisits({
      uri: uri(aUrl),
      visitDate: timeInMicroseconds++,
      transition: PlacesUtils.history.TRANSITION_TYPED,
    });
  }
  info("Add cache.");
  await storeCache(FTP_URL, "testData");
  info("Add form history.");
  await addFormHistory();
  Assert.equal(await getFormHistoryCount(), 1, "Added form history");

  info("Simulate and wait shutdown.");
  await shutdownPlaces();

  Assert.equal(await getFormHistoryCount(), 0, "Form history cleared");

  let stmt = DBConn(true).createStatement(
    "SELECT id FROM moz_places WHERE url = :page_url "
  );

  try {
    URIS.forEach(function(aUrl) {
      stmt.params.page_url = aUrl;
      Assert.ok(!stmt.executeStep());
      stmt.reset();
    });
  } finally {
    stmt.finalize();
  }

  info("Check cache");
  // Check cache.
  await checkCache(FTP_URL);
});

function addFormHistory() {
  return new Promise(resolve => {
    let now = Date.now() * 1000;
    FormHistory.update(
      {
        op: "add",
        fieldname: "testfield",
        value: "test",
        timesUsed: 1,
        firstUsed: now,
        lastUsed: now,
      },
      {
        handleCompletion(reason) {
          resolve();
        },
      }
    );
  });
}

function getFormHistoryCount() {
  return new Promise((resolve, reject) => {
    let count = -1;
    FormHistory.count(
      { fieldname: "testfield" },
      {
        handleResult(result) {
          count = result;
        },
        handleCompletion(reason) {
          resolve(count);
        },
      }
    );
  });
}

function storeCache(aURL, aContent) {
  let cache = Services.cache2;
  let storage = cache.diskCacheStorage(Services.loadContextInfo.default);

  return new Promise(resolve => {
    let storeCacheListener = {
      onCacheEntryCheck(entry) {
        return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED;
      },

      onCacheEntryAvailable(entry, isnew, status) {
        Assert.equal(status, Cr.NS_OK);

        entry.setMetaDataElement("servertype", "0");
        var os = entry.openOutputStream(0, -1);

        var written = os.write(aContent, aContent.length);
        if (written != aContent.length) {
          do_throw(
            "os.write has not written all data!\n" +
              "  Expected: " +
              written +
              "\n" +
              "  Actual: " +
              aContent.length +
              "\n"
          );
        }
        os.close();
        entry.close();
        resolve();
      },
    };

    storage.asyncOpenURI(
      Services.io.newURI(aURL),
      "",
      Ci.nsICacheStorage.OPEN_NORMALLY,
      storeCacheListener
    );
  });
}

function checkCache(aURL) {
  let cache = Services.cache2;
  let storage = cache.diskCacheStorage(Services.loadContextInfo.default);

  return new Promise(resolve => {
    let checkCacheListener = {
      onCacheEntryAvailable(entry, isnew, status) {
        Assert.equal(status, Cr.NS_ERROR_CACHE_KEY_NOT_FOUND);
        resolve();
      },
    };

    storage.asyncOpenURI(
      Services.io.newURI(aURL),
      "",
      Ci.nsICacheStorage.OPEN_READONLY,
      checkCacheListener
    );
  });
}
