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

const TEST_URI = "data:text/html;charset=utf-8,stub generation";
const STUB_FILE = "evaluationResult.js";

add_task(async function() {
  const isStubsUpdate = env.get(STUBS_UPDATE_ENV) == "true";
  info(`${isStubsUpdate ? "Update" : "Check"} ${STUB_FILE}`);

  const generatedStubs = await generateEvaluationResultStubs();

  if (isStubsUpdate) {
    await writeStubsToFile(env, STUB_FILE, generatedStubs);
    ok(true, `${STUB_FILE} was updated`);
    return;
  }

  const existingStubs = getStubFile(STUB_FILE);
  const FAILURE_MSG =
    "The evaluationResult stubs file needs to be updated by running " +
    "`mach test devtools/client/webconsole/test/browser/" +
    "browser_webconsole_stubs_evaluation_result.js --headless " +
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

async function generateEvaluationResultStubs() {
  const stubs = new Map();
  const toolbox = await openNewTabAndToolbox(TEST_URI, "webconsole");
  const webConsoleFront = await toolbox.target.getFront("console");
  for (const [key, code] of getCommands()) {
    const packet = await webConsoleFront.evaluateJSAsync(code);
    stubs.set(key, getCleanedPacket(key, packet));
  }

  return stubs;
}

function getCommands() {
  const evaluationResultCommands = [
    "new Date(0)",
    "asdf()",
    "1 + @",
    "inspect({a: 1})",
    "cd(document)",
    "undefined",
  ];

  const evaluationResult = new Map(
    evaluationResultCommands.map(cmd => [cmd, cmd])
  );
  evaluationResult.set(
    "longString message Error",
    `throw new Error("Long error ".repeat(10000))`
  );

  evaluationResult.set(`eval throw ""`, `throw ""`);
  evaluationResult.set(`eval throw "tomato"`, `throw "tomato"`);
  evaluationResult.set(`eval throw false`, `throw false`);
  evaluationResult.set(`eval throw 0`, `throw 0`);
  evaluationResult.set(`eval throw null`, `throw null`);
  evaluationResult.set(`eval throw undefined`, `throw undefined`);
  evaluationResult.set(`eval throw Symbol`, `throw Symbol("potato")`);
  evaluationResult.set(`eval throw Object`, `throw {vegetable: "cucumber"}`);
  evaluationResult.set(`eval throw Error Object`, `throw new Error("pumpkin")`);
  evaluationResult.set(
    `eval throw Error Object with custom name`,
    `
    var err = new Error("pineapple");
    err.name = "JuicyError";
    err.flavor = "delicious";
    throw err;
  `
  );

  return evaluationResult;
}
