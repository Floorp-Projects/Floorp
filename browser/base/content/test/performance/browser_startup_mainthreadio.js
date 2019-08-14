/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* This test records I/O syscalls done on the main thread during startup.
 *
 * To run this test similar to try server, you need to run:
 *   ./mach package
 *   ./mach test --appname=dist <path to test>
 *
 * If you made changes that cause this test to fail, it's likely because you
 * are touching more files or directories during startup.
 * Most code has no reason to use main thread I/O.
 * If for some reason accessing the file system on the main thread is currently
 * unavoidable, consider defering the I/O as long as you can, ideally after
 * the end of startup.
 * If your code isn't strictly required to show the first browser window,
 * it shouldn't be loaded before we are done with first paint.
 * Finally, if your code isn't really needed during startup, it should not be
 * loaded before we have started handling user events.
 */

"use strict";

const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);

/* Set this to true only for debugging purpose; it makes the output noisy. */
const kDumpAllStacks = false;

// Shortcuts for conditions.
const LINUX = AppConstants.platform == "linux";
const WIN = AppConstants.platform == "win";
const MAC = AppConstants.platform == "macosx";

/* Paths in the whitelist can:
 *  - be a full path, eg. "/etc/mime.types"
 *  - have a prefix which will be resolved using Services.dirsvc
 *    eg. "GreD:omni.ja"
 *    It's possible to have only a prefix, in thise case the directory will
 *    still be resolved, eg. "UAppData:"
 *  - use * at the begining and/or end as a wildcard
 *  - For Windows specific entries that require resolving the path to its
 *    canonical form, ie. the old DOS 8.3 format, use canonicalize: true.
 *    This is needed for stat calls to non-existent files.
 * The folder separator is '/' even for Windows paths, where it'll be
 * automatically converted to '\'.
 *
 * Specifying 'ignoreIfUnused: true' will make the test ignore unused entries;
 * without this the test is strict and will fail if a whitelist entry isn't used.
 *
 * Each entry specifies the maximum number of times an operation is expected to
 * occur.
 * The operations currently reported by the I/O interposer are:
 *   create/open: only supported on Windows currently. The test currently
 *     ignores these markers to have a shorter initial whitelist.
 *     Adding Unix support is bug 1533779.
 *   stat: supported on all platforms when checking the last modified date or
 *     file size. Supported only on Windows when checking if a file exists;
 *     fixing this inconsistency is bug 1536109.
 *   read: supported on all platforms, but unix platforms will only report read
 *     calls going through NSPR.
 *   write: supported on all platforms, but Linux will only report write calls
 *     going through NSPR.
 *   close: supported only on Unix, and only for close calls going through NSPR.
 *     Adding Windows support is bug 1524574.
 *   fsync: supported only on Windows.
 *
 * If an entry specifies more than one operation, if at least one of them is
 * encountered, the test won't report a failure for the entry. This helps when
 * whitelisting cases where the reported operations aren't the same on all
 * platforms due to the I/O interposer inconsistencies across platforms
 * documented above.
 */
const startupPhases = {
  // Anything done before or during app-startup must have a compelling reason
  // to run before we have even selected the user profile.
  "before profile selection": [
    {
      // bug 1541200
      path: "UAppData:Crash Reports/InstallTime20*",
      condition: AppConstants.MOZ_CRASHREPORTER,
      stat: 1, // only caught on Windows.
      read: 1,
      write: 2,
      close: 1,
    },
    {
      // bug 1541200
      path: "UAppData:Crash Reports/LastCrash",
      condition: WIN && AppConstants.MOZ_CRASHREPORTER,
      stat: 1, // only caught on Windows.
      read: 1,
    },
    {
      // bug 1541200
      path: "UAppData:Crash Reports/LastCrash",
      condition: !WIN && AppConstants.MOZ_CRASHREPORTER,
      ignoreIfUnused: true, // only if we ever crashed on this machine
      read: 1,
      close: 1,
    },
    {
      // At least the read seems unavoidable for a regular startup.
      path: "UAppData:profiles.ini",
      ignoreIfUnused: true,
      condition: MAC,
      stat: 1,
      read: 1,
      close: 1,
    },
    {
      // bug 1546931
      path: "UAppData:installs.ini",
      condition: WIN || MAC,
      ignoreIfUnused: true, // only if a real profile exists on the system.
      read: 1,
      stat: 2,
      close: 1,
    },
    {
      // At least the read seems unavoidable for a regular startup.
      path: "UAppData:profiles.ini",
      condition: WIN,
      ignoreIfUnused: true, // only if a real profile exists on the system.
      read: 1,
      stat: 1,
    },
    {
      // bug 1541226, bug 1363586, bug 1541593
      path: "ProfD:",
      condition: WIN,
      stat: 1,
    },
    {
      path: "ProfLD:.startup-incomplete",
      condition: !WIN, // Visible on Windows with an open marker
      close: 1,
    },
    {
      // bug 1541491 to stop using this file, bug 1541494 to write correctly.
      path: "ProfLD:compatibility.ini",
      condition: !WIN, // Visible on Windows with an open marker
      write: 18,
      close: 1,
    },
    {
      path: "GreD:omni.ja",
      condition: !WIN, // Visible on Windows with an open marker
      stat: 1,
    },
    {
      // bug 1376994
      path: "XCurProcD:omni.ja",
      condition: !WIN, // Visible on Windows with an open marker
      stat: 1,
    },
    {
      path: "ProfD:parent.lock",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1541603
      path: "ProfD:minidumps",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1543746
      path: "XCurProcD:defaults/preferences",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1544034
      path: "ProfLDS:startupCache/scriptCache-child-current.bin",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1544034
      path: "ProfLDS:startupCache/scriptCache-child.bin",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1544034
      path: "ProfLDS:startupCache/scriptCache-current.bin",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1544034
      path: "ProfLDS:startupCache/scriptCache.bin",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1541601
      path: "PrfDef:channel-prefs.js",
      stat: 1,
      read: 1,
      close: 1,
    },
    {
      // At least the read seems unavoidable
      path: "PrefD:prefs.js",
      stat: 1,
      read: 1,
      close: 1,
    },
    {
      // bug 1543752
      path: "PrefD:user.js",
      stat: 1,
      read: 1,
      close: 1,
    },
    {
      path: "*ld.so.conf*",
      condition: LINUX,
      ignoreIfUnused: true,
      read: 22,
      close: 11,
    },
    {
      // bug 1546838
      path: "ProfD:xulstore/data.mdb",
      condition: WIN,
      write: 1,
      fsync: 1,
    },
  ],

  "before opening first browser window": [
    {
      // bug 1541226
      path: "ProfD:",
      condition: WIN,
      stat: 1,
    },
    {
      path: "XCurProcD:blocklist.xml",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1534745
      path: "ProfD:cookies.sqlite-journal",
      condition: !LINUX,
      stat: 3,
      write: 4,
    },
    {
      // bug 1534745
      path: "ProfD:cookies.sqlite",
      condition: !LINUX,
      stat: 2,
      read: 2,
      write: 1,
    },
    {
      // bug 1534745
      path: "ProfD:cookies.sqlite-wal",
      condition: WIN,
      stat: 2,
    },
    {
      // bug 975996
      path: "ProfD:permissions.sqlite",
      condition: WIN || MAC,
      fsync: 7,
      read: 2,
      stat: 1,
      write: 10,
    },
    {
      // bug 975996
      path: "ProfD:permissions.sqlite-journal",
      condition: WIN || MAC,
      fsync: 7,
      stat: 26,
      write: 38,
    },
    {
      // bug 975996
      path: "ProfD:permissions.sqlite-wal",
      condition: WIN,
      stat: 20,
    },
    {
      // Seems done by OS X and outside of our control.
      path: "*.savedState/restorecount.plist",
      condition: MAC,
      ignoreIfUnused: true,
      write: 1,
    },
    {
      path: "*ld.so.conf*",
      condition: LINUX,
      ignoreIfUnused: true,
      read: 22,
      close: 11,
    },
    {
      // bug 1545167
      path: "/etc/mime.types",
      condition: LINUX,
      read: 3,
      close: 3,
    },
    {
      // bug 1541246
      path: "ProfD:extensions",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1541246
      path: "XCurProcD:extensions",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1541246
      path: "UAppData:",
      ignoreIfUnused: true, // sometimes before opening first browser window,
      // sometimes before first paint
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1546838
      path: "ProfD:xulstore/data.mdb",
      condition: WIN,
      read: 1,
    },
  ],

  // We reach this phase right after showing the first browser window.
  // This means that any I/O at this point delayed first paint.
  "before first paint": [
    {
      // bug 1541226
      path: "ProfD:",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1545119
      path: "OldUpdRootD:",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1446012
      path: "UpdRootD:updates/0/update.status",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1545123
      path: "ProfD:pluginreg.dat",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1545123
      path: "ProfD:pluginreg.dat.tmp",
      stat: 1,
      write: 64,
      close: 1,
    },
    {
      // bug 1545123
      path: "ProfD:plugins",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1545123
      path: "APlugns:",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1545123
      path: "UserPlugins.parent:",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1545123
      path: "UserPlugins:",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1545123
      path: "ProfD:plugins/nptest.dll",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1545123
      path: "ProfD:plugins/npsecondtest.dll",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1545123
      path: "ProfD:plugins/npthirdtest.dll",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1545123
      path: "ProfD:plugins/npswftest.dll",
      condition: WIN,
      stat: 1,
    },
    {
      path: "XREAppFeat:formautofill@mozilla.org.xpi",
      condition: !WIN,
      stat: 1,
      close: 1,
    },
    {
      // We only hit this for new profiles.
      path: "XREAppDist:distribution.ini",
      condition: WIN,
      stat: 1,
    },
    {
      path: "*WindowsApps/microsoft.windowscommunicationsapps*",
      condition: WIN,
      ignoreIfUnused: true,
      stat: 3,
    },
    {
      // bug 1545139
      path: "*Fonts/StaticCache.dat",
      condition: WIN,
      ignoreIfUnused: true, // Only on Win7
      read: 1,
    },
    {
      // bug 1541246
      path: "UAppData:",
      ignoreIfUnused: true, // sometimes before opening first browser window,
      // sometimes before first paint
      condition: WIN,
      stat: 1,
    },
    {
      // Not in packaged builds; useful for artifact builds.
      path: "GreD:ScalarArtifactDefinitions.json",
      condition: WIN && !AppConstants.MOZILLA_OFFICIAL,
      stat: 1,
    },
    {
      // Not in packaged builds; useful for artifact builds.
      path: "GreD:EventArtifactDefinitions.json",
      condition: WIN && !AppConstants.MOZILLA_OFFICIAL,
      stat: 1,
    },
    {
      // bug 1546838
      path: "ProfD:xulstore/data.mdb",
      condition: MAC,
      write: 3,
    },
  ],

  // We are at this phase once we are ready to handle user events.
  // Any IO at this phase or before gets in the way of the user
  // interacting with the first browser window.
  "before handling user events": [
    {
      path: "GreD:update.test",
      ignoreIfUnused: true,
      condition: LINUX,
      close: 1,
    },
    {
      // bug 1370516 - NSS should be initialized off main thread.
      path: "ProfD:cert9.db",
      condition: WIN,
      read: 2,
      stat: 2,
    },
    {
      // bug 1370516 - NSS should be initialized off main thread.
      path: "ProfD:cert9.db",
      condition: WIN,
      ignoreIfUnused: true, // if canonicalize(ProfD) == ProfD, we'll use the previous entry.
      canonicalize: true,
      stat: 2,
    },
    {
      // bug 1370516 - NSS should be initialized off main thread.
      path: "ProfD:cert9.db-journal",
      condition: WIN,
      canonicalize: true,
      stat: 2,
    },
    {
      // bug 1370516 - NSS should be initialized off main thread.
      path: "ProfD:cert9.db-wal",
      condition: WIN,
      canonicalize: true,
      stat: 2,
    },
    {
      // bug 1370516 - NSS should be initialized off main thread.
      path: "ProfD:pkcs11.txt",
      condition: WIN,
      read: 2,
    },
    {
      // bug 1370516 - NSS should be initialized off main thread.
      path: "ProfD:key4.db",
      condition: WIN,
      read: 2,
      stat: 2,
    },
    {
      // bug 1370516 - NSS should be initialized off main thread.
      path: "ProfD:key4.db",
      condition: WIN,
      ignoreIfUnused: true, // if canonicalize(ProfD) == ProfD, we'll use the previous entry.
      canonicalize: true,
      stat: 2,
    },
    {
      // bug 1370516 - NSS should be initialized off main thread.
      path: "ProfD:key4.db-journal",
      condition: WIN,
      canonicalize: true,
      stat: 5,
    },
    {
      // bug 1370516 - NSS should be initialized off main thread.
      path: "ProfD:key4.db-wal",
      condition: WIN,
      canonicalize: true,
      stat: 5,
    },
    {
      path: "XREAppFeat:webcompat-reporter@mozilla.org.xpi",
      condition: !WIN,
      ignoreIfUnused: true,
      stat: 1,
      close: 1,
    },
    {
      // bug 1003968
      path: "XREAppDist:searchplugins",
      condition: WIN,
      ignoreIfUnused: true, // with WebRender enabled this may happen during "before becoming idle"
      stat: 1,
    },
    {
      path: "XCurProcD:extensions",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1543090
      path: "GreD:omni.ja",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1543090
      path: "XCurProcD:omni.ja",
      condition: WIN,
      stat: 2,
    },
  ],

  // Things that are expected to be completely out of the startup path
  // and loaded lazily when used for the first time by the user should
  // be blacklisted here.
  "before becoming idle": [
    {
      path: "XREAppFeat:screenshots@mozilla.org.xpi",
      ignoreIfUnused: true,
      close: 1,
    },
    {
      path: "XREAppFeat:webcompat-reporter@mozilla.org.xpi",
      ignoreIfUnused: true,
      stat: 1,
      close: 1,
    },
    {
      // bug 1391590
      path: "ProfD:places.sqlite-journal",
      ignoreIfUnused: true,
      fsync: 1,
      stat: 4,
      write: 2,
    },
    {
      // bug 1391590
      path: "ProfD:places.sqlite-wal",
      ignoreIfUnused: true,
      stat: 4,
      fsync: 3,
      write: 148,
    },
    {
      // bug 1391590
      path: "ProfD:places.sqlite-shm",
      condition: WIN,
      ignoreIfUnused: true,
      stat: 1,
    },
    {
      // bug 1391590
      path: "ProfD:places.sqlite",
      ignoreIfUnused: true,
      fsync: 2,
      read: 1,
      stat: 3,
      write: 1310,
    },
    {
      // bug 1391590
      path: "ProfD:favicons.sqlite-journal",
      ignoreIfUnused: true,
      fsync: 2,
      stat: 7,
      write: 7,
    },
    {
      // bug 1391590
      path: "ProfD:favicons.sqlite-wal",
      ignoreIfUnused: true,
      fsync: 2,
      stat: 7,
      write: 15,
    },
    {
      // bug 1391590
      path: "ProfD:favicons.sqlite-shm",
      condition: WIN,
      ignoreIfUnused: true,
      stat: 2,
    },
    {
      // bug 1391590
      path: "ProfD:favicons.sqlite",
      ignoreIfUnused: true,
      fsync: 3,
      read: 4,
      stat: 4,
      write: 1300,
    },
    {
      path: "ProfD:key4.db-journal",
      condition: WIN,
      canonicalize: true,
      stat: 2,
    },
    {
      path: "ProfD:key4.db-wal",
      condition: WIN,
      canonicalize: true,
      stat: 2,
    },
    {
      path: "ProfD:",
      condition: WIN,
      ignoreIfUnused: true,
      stat: 3,
    },
    {
      // bug 1003968
      path: "XREAppDist:searchplugins",
      condition: WIN,
      ignoreIfUnused: true, // with WebRender enabled this may happen during "before handling user events"
      stat: 1,
    },
  ],
};

for (let name of [
  "d3d11layers",
  "d3d9video",
  "glcontext",
  "d3d11video",
  "wmfvpxvideo",
]) {
  startupPhases["before first paint"].push({
    path: `ProfD:${name}.guard`,
    ignoreIfUnused: true,
    stat: 1,
  });
}

function expandWhitelistPath(path, canonicalize = false) {
  if (path.includes(":")) {
    let [prefix, suffix] = path.split(":");
    let [key, property] = prefix.split(".");
    let dir = Services.dirsvc.get(key, Ci.nsIFile);
    if (property) {
      dir = dir[property];
    }

    if (canonicalize) {
      path = dir.QueryInterface(Ci.nsILocalFileWin).canonicalPath;
    } else {
      // Resolve symLinks.
      let dirPath = dir.path;
      while (dir && !dir.isSymlink()) {
        dir = dir.parent;
      }
      if (dir) {
        dirPath = dirPath.replace(dir.path, dir.target);
      }

      path = dirPath;
    }

    if (suffix) {
      path += "/" + suffix;
    }
  }
  if (AppConstants.platform == "win") {
    path = path.replace(/\//g, "\\");
  }
  return path;
}

function getStackFromProfile(profile, stack) {
  const stackPrefixCol = profile.stackTable.schema.prefix;
  const stackFrameCol = profile.stackTable.schema.frame;
  const frameLocationCol = profile.frameTable.schema.location;

  let result = [];
  while (stack) {
    let sp = profile.stackTable.data[stack];
    let frame = profile.frameTable.data[sp[stackFrameCol]];
    stack = sp[stackPrefixCol];
    frame = profile.stringTable[frame[frameLocationCol]];
    if (frame != "js::RunScript" && !frame.startsWith("next (self-hosted:")) {
      result.push(frame);
    }
  }
  return result;
}

function pathMatches(path, filename) {
  path = path.toLowerCase();
  return (
    path == filename || // Full match
    // Wildcard on both sides of the path
    (path.startsWith("*") &&
      path.endsWith("*") &&
      filename.includes(path.slice(1, -1))) ||
    // Wildcard suffix
    (path.endsWith("*") && filename.startsWith(path.slice(0, -1))) ||
    // Wildcard prefix
    (path.startsWith("*") && filename.endsWith(path.slice(1)))
  );
}

add_task(async function() {
  if (
    !AppConstants.NIGHTLY_BUILD &&
    !AppConstants.MOZ_DEV_EDITION &&
    !AppConstants.DEBUG
  ) {
    ok(
      !("@mozilla.org/test/startuprecorder;1" in Cc),
      "the startup recorder component shouldn't exist in this non-nightly/non-devedition/" +
        "non-debug build."
    );
    return;
  }

  {
    let omniJa = Services.dirsvc.get("XCurProcD", Ci.nsIFile);
    omniJa.append("omni.ja");
    if (!omniJa.exists()) {
      ok(
        false,
        "This test requires a packaged build, " +
          "run 'mach package' and then use --appname=dist"
      );
      return;
    }
  }

  let startupRecorder = Cc["@mozilla.org/test/startuprecorder;1"].getService()
    .wrappedJSObject;
  await startupRecorder.done;

  // Add system add-ons to the whitelist dynamically.
  // They should go in the omni.ja file (bug 1357205).
  {
    let addons = await AddonManager.getAddonsByTypes(["extension"]);
    for (let addon of addons) {
      if (addon.isSystem) {
        startupPhases["before opening first browser window"].push({
          path: `XREAppFeat:${addon.id}.xpi`,
          stat: 3,
          close: 2,
        });
        startupPhases["before handling user events"].push({
          path: `XREAppFeat:${addon.id}.xpi`,
          condition: WIN,
          stat: 2,
        });
      }
    }
  }

  // Check for main thread I/O markers in the startup profile.
  let profile = startupRecorder.data.profile.threads[0];

  let phases = {};
  {
    const nameCol = profile.markers.schema.name;
    const dataCol = profile.markers.schema.data;

    let markersForCurrentPhase = [];
    let foundIOMarkers = false;

    for (let m of profile.markers.data) {
      let markerName = profile.stringTable[m[nameCol]];
      if (markerName.startsWith("startupRecorder:")) {
        phases[
          markerName.split("startupRecorder:")[1]
        ] = markersForCurrentPhase;
        markersForCurrentPhase = [];
        continue;
      }

      if (markerName != "FileIO") {
        continue;
      }

      let markerData = m[dataCol];
      if (markerData.source == "sqlite-mainthread") {
        continue;
      }

      let samples = markerData.stack.samples;
      let stack = samples.data[0][samples.schema.stack];
      markersForCurrentPhase.push({
        operation: markerData.operation,
        filename: markerData.filename,
        source: markerData.source,
        stackId: stack,
      });
      foundIOMarkers = true;
    }

    // The I/O interposer is disabled if !RELEASE_OR_BETA, so we expect to have
    // no I/O marker in that case, but it's good to keep the test running to check
    // that we are still able to produce startup profiles.
    is(
      foundIOMarkers,
      !AppConstants.RELEASE_OR_BETA,
      "The IO interposer should be enabled in builds that are not RELEASE_OR_BETA"
    );
    if (!foundIOMarkers) {
      // If a profile unexpectedly contains no I/O marker, it's better to return
      // early to avoid having plenty of confusing "unused whitelist entry" failures.
      return;
    }
  }

  for (let phase in startupPhases) {
    startupPhases[phase] = startupPhases[phase].filter(
      entry => !("condition" in entry) || entry.condition
    );
    startupPhases[phase].forEach(entry => {
      entry.path = expandWhitelistPath(entry.path, entry.canonicalize);
    });
  }

  let tmpPath = expandWhitelistPath("TmpD:").toLowerCase();
  let shouldPass = true;
  for (let phase in phases) {
    let whitelist = startupPhases[phase];
    info(
      `whitelisted paths ${phase}:\n` +
        whitelist
          .map(e => {
            let operations = Object.keys(e)
              .filter(k => k != "path")
              .map(k => `${k}: ${e[k]}`);
            return `  ${e.path} - ${operations.join(", ")}`;
          })
          .join("\n")
    );

    let markers = phases[phase];
    for (let marker of markers) {
      if (marker.operation == "create/open") {
        // TODO: handle these I/O markers once they are supported on
        // non-Windows platforms.
        continue;
      }

      // Convert to lower case before comparing because the OS X test slaves
      // have the 'Firefox' folder in 'Library/Application Support' created
      // as 'firefox' for some reason.
      let filename = marker.filename.toLowerCase();

      if (!filename) {
        // We are still missing the filename on some mainthreadio markers,
        // these markers are currently useless for the purpose of this test.
        continue;
      }

      if (!WIN && filename == "/dev/urandom") {
        continue;
      }

      // /dev/shm is always tmpfs (a memory filesystem); this isn't
      // really I/O any more than mmap/munmap are.
      if (LINUX && filename.startsWith("/dev/shm/")) {
        continue;
      }

      // Shared memory uses temporary files on MacOS <= 10.11 to avoid
      // a kernel security bug that will never be patched (see
      // https://crbug.com/project-zero/1671 for details).  This can
      // be removed when we no longer support those OS versions.
      if (MAC && filename.startsWith(tmpPath + "/org.mozilla.ipc.")) {
        continue;
      }

      let expected = false;
      for (let entry of whitelist) {
        if (pathMatches(entry.path, filename)) {
          entry[marker.operation] = (entry[marker.operation] || 0) - 1;
          entry._used = true;
          expected = true;
          break;
        }
      }
      if (!expected) {
        record(
          false,
          `unexpected ${marker.operation} on ${marker.filename} ${phase}`,
          undefined,
          "  " + getStackFromProfile(profile, marker.stackId).join("\n  ")
        );
        shouldPass = false;
      }
      info(`(${marker.source}) ${marker.operation} - ${marker.filename}`);
      if (kDumpAllStacks) {
        info(
          getStackFromProfile(profile, marker.stackId)
            .map(f => "  " + f)
            .join("\n")
        );
      }
    }

    for (let entry of whitelist) {
      for (let op in entry) {
        if (
          [
            "path",
            "condition",
            "canonicalize",
            "ignoreIfUnused",
            "_used",
          ].includes(op)
        ) {
          continue;
        }
        let message = `${op} on ${entry.path} `;
        if (entry[op] == 0) {
          message += "as many times as expected";
        } else if (entry[op] > 0) {
          message += `allowed ${entry[op]} more times`;
        } else {
          message += `${entry[op] * -1} more times than expected`;
        }
        ok(entry[op] >= 0, `${message} ${phase}`);
      }
      if (!("_used" in entry) && !entry.ignoreIfUnused) {
        ok(false, `unused whitelist entry ${phase}: ${entry.path}`);
        shouldPass = false;
      }
    }
  }

  if (shouldPass) {
    ok(shouldPass, "No unexpected main thread I/O during startup");
  } else {
    const filename = "profile_startup_mainthreadio.json";
    let path = Cc["@mozilla.org/process/environment;1"]
      .getService(Ci.nsIEnvironment)
      .get("MOZ_UPLOAD_DIR");
    let encoder = new TextEncoder();
    let profilePath = OS.Path.join(path, filename);
    await OS.File.writeAtomic(
      profilePath,
      encoder.encode(JSON.stringify(startupRecorder.data.profile))
    );
    ok(
      false,
      "Unexpected main thread I/O behavior during startup; open the " +
        `${filename} artifact in the Firefox Profiler to see what happened`
    );
  }
});
