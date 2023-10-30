/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

SimpleTest.requestCompleteLog();

// Wait until the child process with the given PID has indeed been terminated.
//
// Note that `checkUtilityExists`, and other functions deriving from the output
// of `ChromeUtils.requestProcInfo()`, do not suffice for this purpose! It is an
// attested failure mode that the file-dialog utility process has been removed
// from the proc-info list, but is still live with the file-picker dialog still
// displayed.
function untilChildProcessDead(pid) {
  return utilityProcessTest().untilChildProcessDead(pid);
}

async function fileDialogProcessExists() {
  return !!(await tryGetUtilityPid("windowsFileDialog"));
}

// Poll for the creation of a file dialog process.
function untilFileDialogProcessExists(options = { maxTime: 2000 }) {
  // milliseconds
  const maxTime = options.maxTime ?? 2000,
    pollTime = options.pollTime ?? 100;
  const count = maxTime / pollTime;

  return TestUtils.waitForCondition(
    () => tryGetUtilityPid("windowsFileDialog", { quiet: true }),
    "waiting for file dialog process",
    pollTime, // interval
    count // maxTries
  );
}

function openFileDialog() {
  const process = (async () => {
    await untilFileDialogProcessExists();
    let pid = await tryGetUtilityPid("windowsFileDialog");
    ok(pid, `pid should be acquired in openFileDialog::process (got ${pid})`);
    // HACK: Wait briefly for the file dialog to open.
    //
    // If this is not done, we may attempt to crash the process while it's in
    // the middle of creating and showing the file dialog window. There _should_
    // be no problem with this, but `::MiniDumpWriteDump()` occasionally fails
    // with mysterious errors (`ERROR_BAD_LENGTH`) if we crashed the process
    // while that was happening, yielding no minidump and therefore a failing
    // test.
    //
    // Use of an arbitrary timeout could presumably be avoided by setting a
    // window hook for the file dialog being shown and `await`ing on that.
    //
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(res => setTimeout(res, 1000));
    return pid;
  })();

  const file = new Promise((resolve, reject) => {
    info("Opening Windows file dialog");
    let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
    fp.init(window, "Test: browser_utility_filepicker_crashed.js", fp.modeOpen);
    fp.open(result => {
      ok(
        result == fp.returnCancel,
        "filepicker should resolve to cancellation"
      );
      resolve();
    });
  });

  return { process, file };
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      // remote, no fallback
      ["widget.windows.utility_process_file_picker", 2],
    ],
  });
});

function makeTask(description, Describe, action) {
  let task = async function () {
    if (await fileDialogProcessExists()) {
      // If this test proceeds, it will probably cause whatever other test has a
      // file dialog open to fail.
      //
      // (We shouldn't be running two such tests in parallel on the same Fx
      // instance, but that's not obvious at this level.)
      ok(false, "another test has a file dialog open; aborting");
      return;
    }

    const { process, file } = openFileDialog();
    const pid = await process;
    const untilDead = untilChildProcessDead(pid);

    info(Describe + " the file-dialog utility process");
    await action();

    // the file-picker's callback should have been promptly cancelled
    const _before = Date.now();
    await file;
    const _after = Date.now();
    const delta = _after - _before;
    info(`file callback resolved after ${description} after ${delta}ms`);

    // depending on the test configuration, this may take some time while
    // cleanup occurs
    await untilDead;
  };

  // give this task a legible name
  Object.defineProperty(task, "name", {
    value: "testFileDialogProcess-" + Describe.replace(" ", ""),
  });

  return task;
}

for (let [description, Describe, action] of [
  ["crash", "Crash", () => crashSomeUtilityActor("windowsFileDialog")],
  [
    "being killed",
    "Kill",
    () => cleanUtilityProcessShutdown("windowsFileDialog", true),
  ],
  // Unfortunately, a controlled shutdown doesn't actually terminate the utility
  // process; the file dialog remains open. (This is expected to be resolved with
  // bug 1837008.)
  /* [
    "shutdown",
    "Shut down",
    () => cleanUtilityProcessShutdown("windowsFileDialog"),
  ] */
]) {
  add_task(makeTask(description, Describe, action));
  add_task(testCleanup);
}

async function testCleanup() {
  const killFileDialogProcess = async () => {
    if (await tryGetUtilityPid("windowsFileDialog", { quiet: true })) {
      await cleanUtilityProcessShutdown("windowsFileDialog", true);
      return true;
    }
    return false;
  };

  // If a test failure occurred, the file dialog process may or may not already
  // exist...
  if (await killFileDialogProcess()) {
    console.warn("File dialog process found and killed");
    return;
  }

  // ... and if not, may or may not be pending creation.
  info("Active file dialog process not found; waiting...");
  try {
    await untilFileDialogProcessExists({ maxTime: 1000 });
  } catch (e) {
    info("File dialog process not found during cleanup (as expected)");
    return;
  }
  await killFileDialogProcess();
  console.warn("Delayed file dialog process found and killed");
}
