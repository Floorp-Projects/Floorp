/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Check about:cache after private browsing
// This test covers MozTrap test 6047
// bug 880621

var tmp = {};

function test() {
  waitForExplicitFinish();

  SpecialPowers.pushPrefEnv(
    {
      set: [["privacy.partition.network_state", false]],
    },
    function () {
      Sanitizer.sanitize(["cache"], { ignoreTimespan: false });

      getStorageEntryCount("regular", function (nrEntriesR1) {
        is(nrEntriesR1, 0, "Disk cache reports 0KB and has no entries");

        get_cache_for_private_window();
      });
    }
  );
}

function getStorageEntryCount(device, goon) {
  var storage;
  switch (device) {
    case "private":
      storage = Services.cache2.diskCacheStorage(
        Services.loadContextInfo.private
      );
      break;
    case "regular":
      storage = Services.cache2.diskCacheStorage(
        Services.loadContextInfo.default
      );
      break;
    default:
      throw new Error(`Unknown device ${device} at getStorageEntryCount`);
  }

  var visitor = {
    entryCount: 0,
    onCacheStorageInfo(aEntryCount, aConsumption) {},
    onCacheEntryInfo(uri) {
      var urispec = uri.asciiSpec;
      info(device + ":" + urispec + "\n");
      if (urispec.match(/^https:\/\/example.com\//)) {
        ++this.entryCount;
      }
    },
    onCacheEntryVisitCompleted() {
      goon(this.entryCount);
    },
  };

  storage.asyncVisitStorage(visitor, true);
}

function get_cache_for_private_window() {
  let win = whenNewWindowLoaded({ private: true }, function () {
    executeSoon(function () {
      ok(true, "The private window got loaded");

      let tab = BrowserTestUtils.addTab(win.gBrowser, "https://example.com");
      win.gBrowser.selectedTab = tab;
      let newTabBrowser = win.gBrowser.getBrowserForTab(tab);

      BrowserTestUtils.browserLoaded(newTabBrowser).then(function () {
        executeSoon(function () {
          getStorageEntryCount("private", function (nrEntriesP) {
            Assert.greaterOrEqual(
              nrEntriesP,
              1,
              "Memory cache reports some entries from example.org domain"
            );

            getStorageEntryCount("regular", function (nrEntriesR2) {
              is(nrEntriesR2, 0, "Disk cache reports 0KB and has no entries");

              win.close();
              finish();
            });
          });
        });
      });
    });
  });
}
