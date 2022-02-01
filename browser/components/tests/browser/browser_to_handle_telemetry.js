/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function handleCommandLine(args, state) {
  let newWinPromise;
  let target = args[args.length - 1];

  const EXISTING_FILE = Cc["@mozilla.org/file/local;1"].createInstance(
    Ci.nsIFile
  );
  EXISTING_FILE.initWithPath(getTestFilePath("dummy.pdf"));

  if (!target.includes("://")) {
    // For simplicity, we handle only absolute paths.  We could resolve relative
    // paths, but that would itself require the functionality of the
    // `nsICommandLine` instance we produce using this input.
    const file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
    file.initWithPath(target);
    target = Services.io.newFileURI(file).spec;
  }

  if (state == Ci.nsICommandLine.STATE_INITIAL_LAUNCH) {
    newWinPromise = BrowserTestUtils.waitForNewWindow({
      url: target, // N.b.: trailing slashes matter when matching.
    });
  }

  let cmdLineHandler = Cc["@mozilla.org/browser/final-clh;1"].getService(
    Ci.nsICommandLineHandler
  );

  let fakeCmdLine = Cu.createCommandLine(args, EXISTING_FILE.parent, state);
  cmdLineHandler.handle(fakeCmdLine);

  if (newWinPromise) {
    let newWin = await newWinPromise;
    await BrowserTestUtils.closeWindow(newWin);
  } else {
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
}

function assertToHandleTelemetry(assertions) {
  const scalars = TelemetryTestUtils.getProcessScalars("parent", true, true);

  const { invoked, launched, ...unknown } = assertions;
  if (Object.keys(unknown).length) {
    throw Error(
      `Unknown keys given to assertToHandleTelemetry: ${JSON.stringify(
        unknown
      )}`
    );
  }
  if (invoked === undefined && launched === undefined) {
    throw Error("No known keys given to assertToHandleTelemetry");
  }

  for (let scalar of ["invoked", "launched"]) {
    if (scalar in assertions) {
      const { handled, not_handled } = assertions[scalar] || {};
      if (handled) {
        TelemetryTestUtils.assertKeyedScalar(
          scalars,
          `os.environment.${scalar}_to_handle`,
          handled,
          1,
          `${scalar} to handle '${handled}' 1 times`
        );
        // Intentionally nested.
        if (not_handled) {
          Assert.equal(
            not_handled in scalars[`os.environment.${scalar}_to_handle`],
            false,
            `${scalar} to handle '${not_handled}' 0 times`
          );
        }
      } else {
        TelemetryTestUtils.assertScalarUnset(
          scalars,
          `os.environment.${scalar}_to_handle`
        );

        if (not_handled) {
          throw new Error(
            `In ${scalar}, 'not_handled' is only valid with 'handled'`
          );
        }
      }
    }
  }
}

add_task(async function test_invoked_to_handle_registered_file_type() {
  await handleCommandLine(
    [
      "-osint",
      "-url",
      getTestFilePath("../../../../dom/security/test/csp/dummy.pdf"),
    ],
    Ci.nsICommandLine.STATE_REMOTE_EXPLICIT
  );

  assertToHandleTelemetry({
    invoked: { handled: ".pdf", not_handled: ".html" },
    launched: null,
  });
});

add_task(async function test_invoked_to_handle_unregistered_file_type() {
  await handleCommandLine(
    ["-osint", "-url", getTestFilePath("browser.ini")],
    Ci.nsICommandLine.STATE_REMOTE_EXPLICIT
  );

  assertToHandleTelemetry({
    invoked: { handled: ".<other extension>", not_handled: ".ini" },
    launched: null,
  });
});

add_task(async function test_invoked_to_handle_registered_protocol() {
  await handleCommandLine(
    ["-osint", "-url", "https://example.com/"],
    Ci.nsICommandLine.STATE_REMOTE_EXPLICIT
  );

  assertToHandleTelemetry({
    invoked: { handled: "https", not_handled: "mailto" },
    launched: null,
  });
});

add_task(async function test_invoked_to_handle_unregistered_protocol() {
  // Truly unknown protocols get "URI fixed up" to search provider queries.
  // `ftp` does not get fixed up.
  await handleCommandLine(
    ["-osint", "-url", "ftp://example.com/"],
    Ci.nsICommandLine.STATE_REMOTE_EXPLICIT
  );

  assertToHandleTelemetry({
    invoked: { handled: "<other protocol>", not_handled: "ftp" },
    launched: null,
  });
});

add_task(async function test_launched_to_handle_registered_protocol() {
  await handleCommandLine(
    ["-osint", "-url", "https://example.com/"],
    Ci.nsICommandLine.STATE_INITIAL_LAUNCH
  );

  assertToHandleTelemetry({
    invoked: null,
    launched: { handled: "https", not_handled: "mailto" },
  });
});

add_task(async function test_launched_to_handle_registered_file_type() {
  await handleCommandLine(
    [
      "-osint",
      "-url",
      getTestFilePath("../../../../dom/security/test/csp/dummy.pdf"),
    ],
    Ci.nsICommandLine.STATE_INITIAL_LAUNCH
  );

  assertToHandleTelemetry({
    invoked: null,
    launched: { handled: ".pdf", not_handled: ".html" },
  });
});

add_task(async function test_invoked_no_osint() {
  await handleCommandLine(
    ["-url", "https://example.com/"],
    Ci.nsICommandLine.STATE_REMOTE_EXPLICIT
  );

  assertToHandleTelemetry({
    invoked: null,
    launched: null,
  });
});

add_task(async function test_launched_no_osint() {
  await handleCommandLine(
    ["-url", "https://example.com/"],
    Ci.nsICommandLine.STATE_INITIAL_LAUNCH
  );

  assertToHandleTelemetry({
    invoked: null,
    launched: null,
  });
});
