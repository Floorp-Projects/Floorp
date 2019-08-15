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
 */

"use strict";

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
const processes = {
  "Web Content": [
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
      // Exists call in ScopedXREEmbed::SetAppDir
      path: "XCurProcD:",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1357205
      path: "XREAppFeat:webcompat@mozilla.org.xpi",
      condition: !WIN,
      stat: 1,
    },
    {
      // bug 1357205
      path: "XREAppFeat:formautofill@mozilla.org.xpi",
      condition: !WIN,
      ignoreIfUnused: true,
      stat: 1,
    },
  ],
  "Privileged Content": [
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
      // Exists call in ScopedXREEmbed::SetAppDir
      path: "XCurProcD:",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1357205
      path: "XREAppFeat:webcompat@mozilla.org.xpi",
      condition: !WIN,
      stat: 1,
    },
  ],
  WebExtensions: [
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
      // Exists call in ScopedXREEmbed::SetAppDir
      path: "XCurProcD:",
      condition: WIN,
      stat: 1,
    },
    {
      // bug 1357205
      path: "XREAppFeat:webcompat@mozilla.org.xpi",
      condition: !WIN,
      stat: 1,
    },
    {
      // bug 1357205
      path: "XREAppFeat:formautofill@mozilla.org.xpi",
      condition: !WIN,
      stat: 1,
    },
    {
      // bug 1357205
      path: "XREAppFeat:screenshots@mozilla.org.xpi",
      condition: !WIN,
      close: 1,
    },
  ],
};

function expandWhitelistPath(path) {
  if (path.includes(":")) {
    let [prefix, suffix] = path.split(":");
    let [key, property] = prefix.split(".");
    let dir = Services.dirsvc.get(key, Ci.nsIFile);
    if (property) {
      dir = dir[property];
    }

    // Resolve symLinks.
    let dirPath = dir.path;
    while (dir && !dir.isSymlink()) {
      dir = dir.parent;
    }
    if (dir) {
      dirPath = dirPath.replace(dir.path, dir.target);
    }

    path = dirPath;

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

function getIOMarkersFromProfile(profile) {
  const nameCol = profile.markers.schema.name;
  const dataCol = profile.markers.schema.data;

  let markers = [];
  for (let m of profile.markers.data) {
    let markerName = profile.stringTable[m[nameCol]];

    if (markerName != "FileIO") {
      continue;
    }

    let markerData = m[dataCol];
    if (markerData.source == "sqlite-mainthread") {
      continue;
    }

    let samples = markerData.stack.samples;
    let stack = samples.data[0][samples.schema.stack];
    markers.push({
      operation: markerData.operation,
      filename: markerData.filename,
      source: markerData.source,
      stackId: stack,
    });
  }

  return markers;
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

  for (let process in processes) {
    processes[process] = processes[process].filter(
      entry => !("condition" in entry) || entry.condition
    );
    processes[process].forEach(entry => {
      entry.path = expandWhitelistPath(entry.path, entry.canonicalize);
    });
  }

  let tmpPath = expandWhitelistPath("TmpD:").toLowerCase();
  let shouldPass = true;
  for (let procName in processes) {
    let whitelist = processes[procName];
    info(
      `whitelisted paths for ${procName} process:\n` +
        whitelist
          .map(e => {
            let operations = Object.keys(e)
              .filter(k => !["path", "condition"].includes(k))
              .map(k => `${k}: ${e[k]}`);
            return `  ${e.path} - ${operations.join(", ")}`;
          })
          .join("\n")
    );

    let profile;
    for (let process of startupRecorder.data.profile.processes) {
      if (process.threads[0].processName == procName) {
        profile = process.threads[0];
        break;
      }
    }
    if (procName == "Privileged Content" && !profile) {
      // The Privileged Content is started from an idle task that may not have
      // been executed yet at the time we captured the startup profile in
      // startupRecorder.
      todo(false, `profile for ${procName} process not found`);
    } else {
      ok(profile, `Found profile for ${procName} process`);
    }
    if (!profile) {
      continue;
    }

    let markers = getIOMarkersFromProfile(profile);
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
          `unexpected ${marker.operation} on ${
            marker.filename
          } in ${procName} process`,
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

    if (!whitelist.length) {
      continue;
    }
    // The I/O interposer is disabled if !RELEASE_OR_BETA, so we expect to have
    // no I/O marker in that case, but it's good to keep the test running to check
    // that we are still able to produce startup profiles.
    is(
      markers.length > 0,
      !AppConstants.RELEASE_OR_BETA,
      procName +
        " startup profiles should have IO markers in builds that are not RELEASE_OR_BETA"
    );
    if (!markers.length) {
      // If a profile unexpectedly contains no I/O marker, avoid generating
      // plenty of confusing "unused whitelist entry" failures.
      continue;
    }

    for (let entry of whitelist) {
      for (let op in entry) {
        if (["path", "condition", "ignoreIfUnused", "_used"].includes(op)) {
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
        ok(entry[op] >= 0, `${message} in ${procName} process`);
      }
      if (!("_used" in entry) && !entry.ignoreIfUnused) {
        ok(false, `unused whitelist entry ${procName}: ${entry.path}`);
        shouldPass = false;
      }
    }
  }

  if (shouldPass) {
    ok(shouldPass, "No unexpected main thread I/O during startup");
  } else {
    const filename = "profile_startup_content_mainthreadio.json";
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
      "Unexpected main thread I/O behavior during child process startup; " +
        `open the ${filename} artifact in the Firefox Profiler to see what happened`
    );
  }
});
