/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  STUBS_UPDATE_ENV,
  getCleanedPacket,
  getSerializedPacket,
  getStubFile,
  writeStubsToFile,
} = require(`${CHROME_URL_ROOT}stub-generator-helpers`);

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
    "The evaluationResult stubs file needs to be updated by running `" +
    `mach test ${getCurrentTestFilePath()} --headless --setenv WEBCONSOLE_STUBS_UPDATE=true` +
    "`";

  if (generatedStubs.size !== existingStubs.rawPackets.size) {
    ok(false, FAILURE_MSG);
    return;
  }

  let failed = false;
  for (const [key, packet] of generatedStubs) {
    const packetStr = getSerializedPacket(packet, {
      sortKeys: true,
      replaceActorIds: true,
    });
    const existingPacketStr = getSerializedPacket(
      existingStubs.rawPackets.get(key),
      { sortKeys: true, replaceActorIds: true }
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
  for (const [key, code] of getCommands()) {
    const packet = await toolbox.commands.scriptCommand.execute(code);
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
  evaluationResult.set(
    `eval throw Error Object with error cause`,
    `
    var originalError = new SyntaxError("original error")
    var err = new Error("something went wrong", {
      cause: originalError
    });
    throw err;
  `
  );
  evaluationResult.set(
    `eval throw Error Object with cause chain`,
    `
    var errA = new Error("err-a")
    var errB = new Error("err-b", { cause: errA })
    var errC = new Error("err-c", { cause: errB })
    var errD = new Error("err-d", { cause: errC })
    throw errD;
  `
  );
  evaluationResult.set(
    `eval throw Error Object with cyclical cause chain`,
    `
    var errX = new Error("err-x", { cause: errY})
    var errY = new Error("err-y", { cause: errX })
    throw errY;
  `
  );
  evaluationResult.set(
    `eval throw Error Object with falsy cause`,
    `throw new Error("false cause", { cause: false });`
  );
  evaluationResult.set(
    `eval throw Error Object with null cause`,
    `throw new Error("null cause", { cause: null });`
  );
  evaluationResult.set(
    `eval throw Error Object with undefined cause`,
    `throw new Error("undefined cause", { cause: undefined });`
  );
  evaluationResult.set(
    `eval throw Error Object with number cause`,
    `throw new Error("number cause", { cause: 0 });`
  );
  evaluationResult.set(
    `eval throw Error Object with string cause`,
    `throw new Error("string cause", { cause: "cause message" });`
  );
  evaluationResult.set(
    `eval throw Error Object with object cause`,
    `throw new Error("object cause", { cause: { code: 234, message: "ERR_234"} });`
  );

  evaluationResult.set(`eval pending promise`, `new Promise(() => {})`);
  evaluationResult.set(`eval Promise.resolve`, `Promise.resolve(123)`);
  evaluationResult.set(`eval Promise.reject`, `Promise.reject("ouch")`);
  evaluationResult.set(
    `eval resolved promise`,
    `Promise.resolve().then(() => 246)`
  );
  evaluationResult.set(
    `eval rejected promise`,
    `Promise.resolve().then(() => a.b.c)`
  );
  evaluationResult.set(
    `eval rejected promise with Error`,
    `Promise.resolve().then(() => {
      try {
        a.b.c
      } catch(e) {
        throw new Error("something went wrong", { cause: e })
      }
    })`
  );

  return evaluationResult;
}
