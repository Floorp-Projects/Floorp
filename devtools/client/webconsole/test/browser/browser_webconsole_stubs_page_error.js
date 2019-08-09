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

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/test/browser/test-console-api.html";

add_task(async function() {
  const isStubsUpdate = env.get(STUBS_UPDATE_ENV) == "true";
  const filePath = getStubFilePath("pageError.js", env);
  info(`${isStubsUpdate ? "Update" : "Check"} stubs at ${filePath}`);

  const generatedStubs = await generatePageErrorStubs();

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
      "The pageError stubs file needs to be updated by running " +
        "`mach test devtools/client/webconsole/test/browser/" +
        "browser_webconsole_stubs_page_error.js --headless " +
        "--setenv WEBCONSOLE_STUBS_UPDATE=true`"
    );
  }
});

async function generatePageErrorStubs() {
  const stubs = {
    preparedMessages: [],
    packets: [],
  };

  const toolbox = await openNewTabAndToolbox(TEST_URI, "webconsole");

  for (const [key, code] of getCommands()) {
    const onPageError = toolbox.target.activeConsole.once("pageError");

    // On e10s, the exception is triggered in child process
    // and is ignored by test harness
    // expectUncaughtException should be called for each uncaught exception.
    if (!Services.appinfo.browserTabsRemoteAutostart) {
      expectUncaughtException();
    }

    await ContentTask.spawn(gBrowser.selectedBrowser, code, function(subCode) {
      const script = content.document.createElement("script");
      script.append(content.document.createTextNode(subCode));
      content.document.body.append(script);
      script.remove();
    });

    const packet = await onPageError;
    stubs.packets.push(formatPacket(key, packet));
    stubs.preparedMessages.push(formatStub(key, packet));
  }

  await closeTabAndToolbox();
  return formatFile(stubs, "ConsoleMessage");
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
