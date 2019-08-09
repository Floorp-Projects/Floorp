/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  STUBS_UPDATE_ENV,
  formatPacket,
  formatStub,
  formatFile,
  getStubFilePath,
} = require("devtools/client/webconsole/test/browser/stub-generator-helpers");

const TEST_URI = "data:text/html;charset=utf-8,stub generation";

add_task(async function() {
  const isStubsUpdate = env.get(STUBS_UPDATE_ENV) == "true";
  const filePath = getStubFilePath("evaluationResult.js", env);
  info(`${isStubsUpdate ? "Update" : "Check"} stubs at ${filePath}`);

  const generatedStubs = await generateEvaluationResultStubs();

  if (isStubsUpdate) {
    await OS.File.writeAtomic(filePath, generatedStubs);
    ok(true, `${filePath} was successfully updated`);
    return;
  }

  const repoStubFileContent = await OS.File.read(filePath, {
    encoding: "utf-8",
  });
  is(generatedStubs, repoStubFileContent, "stubs file is up to date");

  if (generatedStubs != repoStubFileContent) {
    ok(
      false,
      "The evaluationResult stubs file needs to be updated by running " +
        "`mach test devtools/client/webconsole/test/browser/" +
        "browser_webconsole_stubs_evaluation_result.js --headless " +
        "--setenv WEBCONSOLE_STUBS_UPDATE=true`"
    );
  }
});

async function generateEvaluationResultStubs() {
  const stubs = {
    preparedMessages: [],
    packets: [],
  };

  const toolbox = await openNewTabAndToolbox(TEST_URI, "webconsole");

  for (const [key, code] of getCommands()) {
    const packet = await toolbox.target.activeConsole.evaluateJS(code);

    stubs.packets.push(formatPacket(key, packet));
    stubs.preparedMessages.push(formatStub(key, packet));
  }

  await closeTabAndToolbox();
  return formatFile(stubs, "ConsoleMessage");
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
  return evaluationResult;
}
