/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/**
 * Tests that a chrome debugger can be created in a new process.
 */

"use strict";

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
PromiseTestUtils.allowMatchingRejectionsGlobally(/File closed/);
PromiseTestUtils.allowMatchingRejectionsGlobally(/NS_ERROR_FAILURE/);

// This test can be slow to run
requestLongerTimeout(5);

const { BrowserToolboxLauncher } = ChromeUtils.importESModule(
  "resource://devtools/client/framework/browser-toolbox/Launcher.sys.mjs"
);

add_task(async function() {
  await pushPref("devtools.chrome.enabled", true);
  await pushPref("devtools.debugger.remote-enabled", true);

  info("Call BrowserToolboxLauncher.init");
  const [browserToolboxLauncher, process, profilePath] = await new Promise(
    resolve => {
      BrowserToolboxLauncher.init({
        onRun: (btl, dbgProcess, dbgProfilePath) => {
          info("Browser toolbox process started successfully.");
          resolve([btl, dbgProcess, dbgProfilePath]);
        },
      });
    }
  );

  ok(process, "The remote debugger process was created");
  ok(process.exitCode == null, "The remote debugger process is running");
  is(
    typeof process.pid,
    "number",
    `The remote debugger process has a proper pid (${process.pid})`
  );

  is(
    profilePath,
    PathUtils.join(PathUtils.profileDir, "chrome_debugger_profile"),
    `The remote debugger profile has the expected path`
  );

  info("Close the browser toolbox");
  await browserToolboxLauncher.close();

  is(
    process.exitCode,
    Services.appinfo.OS == "WINNT" ? -9 : -15,
    "The remote debugger process died cleanly"
  );
});
