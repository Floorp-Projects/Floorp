/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  STUBS_UPDATE_ENV,
  getCleanedPacket,
  getSerializedPacket,
  getStubFilePath,
  writeStubsToFile,
} = require("devtools/client/webconsole/test/browser/stub-generator-helpers");

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/test/browser/test-console-api.html";
const STUB_FILE = "pageError.js";

add_task(async function() {
  await pushPref("javascript.options.asyncstack", true);

  const isStubsUpdate = env.get(STUBS_UPDATE_ENV) == "true";
  info(`${isStubsUpdate ? "Update" : "Check"} ${STUB_FILE}`);

  const generatedStubs = await generatePageErrorStubs();

  if (isStubsUpdate) {
    await writeStubsToFile(
      getStubFilePath(STUB_FILE, env, true),
      generatedStubs
    );
    ok(true, `${STUB_FILE} was updated`);
    return;
  }

  const existingStubs = require(getStubFilePath(STUB_FILE));
  const FAILURE_MSG =
    "The pageError stubs file needs to be updated by running " +
    "`mach test devtools/client/webconsole/test/browser/" +
    "browser_webconsole_stubs_page_error.js --headless " +
    "--setenv WEBCONSOLE_STUBS_UPDATE=true`";

  if (generatedStubs.size !== existingStubs.rawPackets.size) {
    ok(false, FAILURE_MSG);
    return;
  }

  let failed = false;
  for (const [key, packet] of generatedStubs) {
    const packetStr = getSerializedPacket(packet);
    const existingPacketStr = getSerializedPacket(
      existingStubs.rawPackets.get(key)
    );
    is(packetStr, existingPacketStr, `"${key}" packet has expected value`);
    failed = failed || packetStr !== existingPacketStr;
  }

  if (failed) {
    ok(false, FAILURE_MSG);
  } else {
    ok(true, "Stubs are up to date");
  }

  await closeTabAndToolbox();
});

async function generatePageErrorStubs() {
  const stubs = new Map();
  const toolbox = await openNewTabAndToolbox(TEST_URI, "webconsole");
  const webConsoleFront = await toolbox.target.getFront("console");
  for (const [key, code] of getCommands()) {
    const onPageError = webConsoleFront.once("pageError");

    // On e10s, the exception is triggered in child process
    // and is ignored by test harness
    // expectUncaughtException should be called for each uncaught exception.
    if (!Services.appinfo.browserTabsRemoteAutostart) {
      expectUncaughtException();
    }

    // Note: This needs to use ContentTask rather than SpecialPowers.spawn
    // because the latter includes cross-process stack information.
    await ContentTask.spawn(gBrowser.selectedBrowser, code, function(subCode) {
      const script = content.document.createElement("script");
      script.append(content.document.createTextNode(subCode));
      content.document.body.append(script);
      script.remove();
    });

    const packet = await onPageError;
    stubs.set(key, getCleanedPacket(key, packet));
  }

  return stubs;
}

function getCommands() {
  const pageError = new Map();

  pageError.set(
    "ReferenceError: asdf is not defined",
    `
  function bar() {
    asdf()
  }
  function foo() {
    bar()
  }

  foo()
`
  );

  pageError.set(
    "SyntaxError: redeclaration of let a",
    `
  let a, a;
`
  );

  pageError.set(
    "TypeError longString message",
    `throw new Error("Long error ".repeat(10000))`
  );

  pageError.set(`throw ""`, `throw ""`);
  pageError.set(`throw "tomato"`, `throw "tomato"`);
  return pageError;
}
