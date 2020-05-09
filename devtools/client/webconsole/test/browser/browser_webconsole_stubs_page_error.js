/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  STUBS_UPDATE_ENV,
  getCleanedPacket,
  getSerializedPacket,
  getStubFile,
  writeStubsToFile,
} = require("chrome://mochitests/content/browser/devtools/client/webconsole/test/browser/stub-generator-helpers");

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/test/browser/test-console-api.html";
const STUB_FILE = "pageError.js";

add_task(async function() {
  await pushPref("javascript.options.asyncstack", true);

  const isStubsUpdate = env.get(STUBS_UPDATE_ENV) == "true";
  info(`${isStubsUpdate ? "Update" : "Check"} ${STUB_FILE}`);

  const generatedStubs = await generatePageErrorStubs();

  if (isStubsUpdate) {
    await writeStubsToFile(env, STUB_FILE, generatedStubs);
    ok(true, `${STUB_FILE} was updated`);
    return;
  }

  const existingStubs = getStubFile(STUB_FILE);
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

  const evilDomain = `https://evil.com/?`;
  const badDomain = `https://not-so-evil.com/?`;
  const paramLength = 200;
  const longParam = "a".repeat(paramLength);

  const evilURL = `${evilDomain}${longParam}`;
  const badURL = `${badDomain}${longParam}`;

  pageError.set(
    `throw string with URL`,
    `throw "“${evilURL}“ is evil and “${badURL}“ is not good either"`
  );

  pageError.set(`throw ""`, `throw ""`);
  pageError.set(`throw "tomato"`, `throw "tomato"`);
  pageError.set(`throw false`, `throw false`);
  pageError.set(`throw 0`, `throw 0`);
  pageError.set(`throw null`, `throw null`);
  pageError.set(`throw undefined`, `throw undefined`);
  pageError.set(`throw Symbol`, `throw Symbol("potato")`);
  pageError.set(`throw Object`, `throw {vegetable: "cucumber"}`);
  pageError.set(`throw Error Object`, `throw new Error("pumpkin")`);
  pageError.set(
    `throw Error Object with custom name`,
    `
    var err = new Error("pineapple");
    err.name = "JuicyError";
    err.flavor = "delicious";
    throw err;
  `
  );
  pageError.set(`Promise reject ""`, `Promise.reject("")`);
  pageError.set(`Promise reject "tomato"`, `Promise.reject("tomato")`);
  pageError.set(`Promise reject false`, `Promise.reject(false)`);
  pageError.set(`Promise reject 0`, `Promise.reject(0)`);
  pageError.set(`Promise reject null`, `Promise.reject(null)`);
  pageError.set(`Promise reject undefined`, `Promise.reject(undefined)`);
  pageError.set(`Promise reject Symbol`, `Promise.reject(Symbol("potato"))`);
  pageError.set(
    `Promise reject Object`,
    `Promise.reject({vegetable: "cucumber"})`
  );
  pageError.set(
    `Promise reject Error Object`,
    `Promise.reject(new Error("pumpkin"))`
  );
  pageError.set(
    `Promise reject Error Object with custom name`,
    `
    var err = new Error("pineapple");
    err.name = "JuicyError";
    err.flavor = "delicious";
    Promise.reject(err);
  `
  );
  return pageError;
}
