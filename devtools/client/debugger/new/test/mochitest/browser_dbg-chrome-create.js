/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that a chrome debugger can be created in a new process.
 */

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = scopedCuImport(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.whitelistRejectionsGlobally(/File closed/);
PromiseTestUtils.whitelistRejectionsGlobally(/NS_ERROR_FAILURE/);

requestLongerTimeout(5);

const { BrowserToolboxProcess } = ChromeUtils.import(
  "resource://devtools/client/framework/ToolboxProcess.jsm",
  {}
);
let gProcess = undefined;

function initChromeDebugger() {
  info("Initializing a chrome debugger process.");
  return new Promise(resolve => {
    BrowserToolboxProcess.init(onClose, _process => {
      info("Browser toolbox process started successfully.");
      resolve(_process);
    });
  });
}

function onClose() {
  is(
    gProcess._dbgProcess.exitCode,
    Services.appinfo.OS == "WINNT" ? -9 : -15,
    "The remote debugger process didn't die cleanly."
  );

  info("process exit value: " + gProcess._dbgProcess.exitCode);

  info("profile path: " + gProcess._dbgProfilePath);

  finish();
}

registerCleanupFunction(function() {
  gProcess = null;
});

add_task(async function() {
  // Windows XP and 8.1 test slaves are terribly slow at this test.
  await pushPref("devtools.chrome.enabled", true);
  await pushPref("devtools.debugger.remote-enabled", true);

  gProcess = await initChromeDebugger();

  ok(
    gProcess._dbgProcess,
    "The remote debugger process wasn't created properly!"
  );
  ok(
    gProcess._dbgProcess.exitCode == null,
    "The remote debugger process isn't running!"
  );
  is(
    typeof gProcess._dbgProcess.pid,
    "number",
    "The remote debugger process doesn't have a pid (?!)"
  );

  info("process location: " + gProcess._dbgProcess.location);
  info("process pid: " + gProcess._dbgProcess.pid);
  info("process name: " + gProcess._dbgProcess.processName);
  info("process sig: " + gProcess._dbgProcess.processSignature);

  ok(
    gProcess._dbgProfilePath,
    "The remote debugger profile wasn't created properly!"
  );

  is(
    gProcess._dbgProfilePath,
    OS.Path.join(OS.Constants.Path.profileDir, "chrome_debugger_profile"),
    "The remote debugger profile isn't where we expect it!"
  );

  info("profile path: " + gProcess._dbgProfilePath);

  await gProcess.close();
});
