/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

if (SpecialPowers.useRemoteSubframes) {
  requestLongerTimeout(2);
}

const DEFAULT_PROCESS_COUNT = Services.prefs
  .getDefaultBranch(null)
  .getIntPref("dom.ipc.processCount");

/**
 * A test that checks whether any preference getter from the given list
 * of stats was called more often than the max parameter.
 *
 * @param {Array}  stats - an array of [prefName, accessCount] tuples
 * @param {Number} max - the maximum number of times any of the prefs should
 *                 have been called.
 * @param {Object} knownProblematicPrefs (optional) - an object that defines
 *                 prefs that should be exempt from checking the
 *                 maximum access. It looks like the following:
 *
 *                 pref_name: {
 *                   min: [Number] the minimum amount of times this should have
 *                                 been called (to avoid keeping around dead items)
 *                   max: [Number] the maximum amount of times this should have
 *                                 been called (to avoid this creeping up further)
 *                 }
 */
function checkPrefGetters(stats, max, knownProblematicPrefs = {}) {
  let getterStats = Object.entries(stats).sort(
    ([, val1], [, val2]) => val2 - val1
  );

  // Clone the list to be able to delete entries to check if we
  // forgot any later on.
  knownProblematicPrefs = Object.assign({}, knownProblematicPrefs);

  for (let [pref, count] of getterStats) {
    let prefLimits = knownProblematicPrefs[pref];
    if (!prefLimits) {
      Assert.lessOrEqual(
        count,
        max,
        `${pref} should not be accessed more than ${max} times.`
      );
    } else {
      // Still record how much this pref was accessed even if we don't do any real assertions.
      if (!prefLimits.min && !prefLimits.max) {
        info(
          `${pref} should not be accessed more than ${max} times and was accessed ${count} times.`
        );
      }

      if (prefLimits.min) {
        Assert.lessOrEqual(
          prefLimits.min,
          count,
          `${pref} should be accessed at least ${prefLimits.min} times.`
        );
      }
      if (prefLimits.max) {
        Assert.lessOrEqual(
          count,
          prefLimits.max,
          `${pref} should be accessed at most ${prefLimits.max} times.`
        );
      }
      delete knownProblematicPrefs[pref];
    }
  }

  // This pref will be accessed by mozJSComponentLoader when loading modules,
  // which fails TV runs since they run the test multiple times without restarting.
  // We just ignore this pref, since it's for testing only anyway.
  if (knownProblematicPrefs["browser.startup.record"]) {
    delete knownProblematicPrefs["browser.startup.record"];
  }

  let unusedPrefs = Object.keys(knownProblematicPrefs);
  is(
    unusedPrefs.length,
    0,
    `Should have accessed all known problematic prefs. Remaining: ${unusedPrefs}`
  );
}

/**
 * A helper function to read preference access data
 * using the Services.prefs.readStats() function.
 */
function getPreferenceStats() {
  let stats = {};
  Services.prefs.readStats((key, value) => (stats[key] = value));
  return stats;
}

add_task(async function debug_only() {
  ok(AppConstants.DEBUG, "You need to run this test on a debug build.");
});

// Just checks how many prefs were accessed during startup.
add_task(async function startup() {
  let max = 40;

  let knownProblematicPrefs = {
    "browser.startup.record": {
      min: 200,
      max: 350,
    },
    "layout.css.dpi": {
      min: 45,
      max: 81,
    },
    "network.loadinfo.skip_type_assertion": {
      // This is accessed in debug only.
    },
    "extensions.getAddons.cache.enabled": {
      min: 4,
      max: 55,
    },
    "chrome.override_package.global": {
      min: 0,
      max: 50,
    },
  };

  let startupRecorder = Cc["@mozilla.org/test/startuprecorder;1"].getService()
    .wrappedJSObject;
  await startupRecorder.done;

  ok(startupRecorder.data.prefStats, "startupRecorder has prefStats");

  checkPrefGetters(startupRecorder.data.prefStats, max, knownProblematicPrefs);
});

// This opens 10 tabs and checks pref getters.
add_task(async function open_10_tabs() {
  // This is somewhat arbitrary. When we had a default of 4 content processes
  // the value was 15. We need to scale it as we increase the number of
  // content processes so we approximate with 4 * process_count.
  const max = 4 * DEFAULT_PROCESS_COUNT;

  let knownProblematicPrefs = {
    "layout.css.dpi": {
      max: 60,
    },
    "browser.zoom.full": {
      min: 10,
      max: 25,
    },
    "browser.startup.record": {
      max: 20,
    },
    "browser.tabs.remote.logSwitchTiming": {
      max: 35,
    },
    "network.loadinfo.skip_type_assertion": {
      // This is accessed in debug only.
    },
  };

  Services.prefs.resetStats();

  let tabs = [];
  while (tabs.length < 10) {
    tabs.push(
      await BrowserTestUtils.openNewForegroundTab(
        gBrowser,
        "http://example.com",
        true,
        true
      )
    );
  }

  for (let tab of tabs) {
    await BrowserTestUtils.removeTab(tab);
  }

  checkPrefGetters(getPreferenceStats(), max, knownProblematicPrefs);
});

// This navigates to 50 sites and checks pref getters.
add_task(async function navigate_around() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // Disable bfcache so that we can measure more accurately the number of
      // pref accesses in the child processes.
      // If bfcache is enabled on Fission
      // dom.ipc.keepProcessesAlive.webIsolated.perOrigin and
      // security.sandbox.content.force-namespace are accessed only a couple of
      // times.
      ["browser.sessionhistory.max_total_viewers", 0],
    ],
  });

  let max = 40;

  let knownProblematicPrefs = {
    "browser.zoom.full": {
      min: 100,
      max: 110,
    },
    "network.loadinfo.skip_type_assertion": {
      // This is accessed in debug only.
    },
  };

  if (AppConstants.NIGHTLY_BUILD) {
    knownProblematicPrefs["toolkit.telemetry.cachedClientID"] = {
      // Bug 1712391: Only an issue in tests where pref is not populated early on
      // in startup. Code path is only accessed in Nightly builds.
    };
  }

  if (SpecialPowers.useRemoteSubframes) {
    // We access this when considering starting a new content process.
    // Because there is no complete list of content process types,
    // caching this is not trivial. Opening 50 different content
    // processes and throwing them away immediately is a bit artificial;
    // we're more likely to keep some around so this shouldn't be quite
    // this bad in practice. Fixing this is
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1600266
    knownProblematicPrefs["dom.ipc.processCount.webIsolated"] = {
      min: 50,
      max: 51,
    };
    // This pref is only accessed in automation to speed up tests.
    knownProblematicPrefs[
      "dom.ipc.keepProcessesAlive.webIsolated.perOrigin"
    ] = {
      min: 50,
      max: 51,
    };
    if (AppConstants.platform == "linux") {
      // The following sandbox pref is covered by
      // https://bugzilla.mozilla.org/show_bug.cgi?id=1600189
      knownProblematicPrefs["security.sandbox.content.force-namespace"] = {
        min: 49,
        max: 55,
      };
      // This was previously being read in the content process, but
      // bug 1725573 moved it into the parent process.  We also block
      // the main thread on requests to the X server, which is likely
      // more problematic than the pref read.  These issues are covered
      // by https://bugzilla.mozilla.org/show_bug.cgi?id=1729080
      knownProblematicPrefs["gfx.color_management.display_profile"] = {
        min: 49,
        max: 50,
      };
    } else if (AppConstants.platform == "win") {
      // The following 2 graphics prefs are covered by
      // https://bugzilla.mozilla.org/show_bug.cgi?id=1639497
      knownProblematicPrefs["gfx.canvas.azure.backends"] = {
        min: 100,
        max: 110,
      };
      knownProblematicPrefs["gfx.content.azure.backends"] = {
        min: 100,
        max: 110,
      };
      // The following 2 sandbox prefs are covered by
      // https://bugzilla.mozilla.org/show_bug.cgi?id=1639494
      knownProblematicPrefs["security.sandbox.content.read_path_whitelist"] = {
        min: 49,
        max: 55,
      };
      knownProblematicPrefs["security.sandbox.logging.enabled"] = {
        min: 49,
        max: 55,
      };
    }
  }

  Services.prefs.resetStats();

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com",
    true,
    true
  );

  let urls = [
    "http://example.com/",
    "https://example.com/",
    "http://example.org/",
    "https://example.org/",
  ];

  for (let i = 0; i < 50; i++) {
    let url = urls[i % urls.length];
    info(`Navigating to ${url}...`);
    BrowserTestUtils.loadURI(tab.linkedBrowser, url);
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, url);
    info(`Loaded ${url}.`);
  }

  await BrowserTestUtils.removeTab(tab);

  checkPrefGetters(getPreferenceStats(), max, knownProblematicPrefs);
});
