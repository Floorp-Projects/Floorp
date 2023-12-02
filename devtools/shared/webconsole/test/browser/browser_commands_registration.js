/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for Web Console commands registration.

add_task(async function () {
  const tab = await addTab("data:text/html,<div id=quack></div>");

  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();

  // Fetch WebConsoleCommandsManager so that it is available for next Content Tasks
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function () {
    const { require } = ChromeUtils.importESModule(
      "resource://devtools/shared/loader/Loader.sys.mjs"
    );
    const {
      WebConsoleCommandsManager,
    } = require("resource://devtools/server/actors/webconsole/commands/manager.js");

    // Bind the symbol on this in order to make it available for next tasks
    this.WebConsoleCommandsManager = WebConsoleCommandsManager;
  });

  await registerNewCommand(commands);
  await registerAccessor(commands);
});

async function evaluateJSAndCheckResult(commands, input, expected) {
  const response = await commands.scriptCommand.execute(input);
  checkObject(response, expected);
}

async function registerNewCommand(commands) {
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function () {
    this.WebConsoleCommandsManager.register({
      name: "setFoo",
      isSideEffectFree: false,
      command(owner, value) {
        owner.window.foo = value;
        return "ok";
      },
    });
  });

  const command = "setFoo('bar')";
  await evaluateJSAndCheckResult(commands, command, {
    input: command,
    result: "ok",
  });

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function () {
    is(content.top.foo, "bar", "top.foo should equal to 'bar'");
  });
}

async function registerAccessor(commands) {
  await ContentTask.spawn(gBrowser.selectedBrowser, null, function () {
    this.WebConsoleCommandsManager.register({
      name: "$foo",
      isSideEffectFree: true,
      command: {
        get(owner) {
          const foo = owner.window.document.getElementById("quack");
          return owner.makeDebuggeeValue(foo);
        },
      },
    });
  });

  const command = "$foo.textContent = '>o_/'";
  await evaluateJSAndCheckResult(commands, command, {
    input: command,
    result: ">o_/",
  });

  await ContentTask.spawn(gBrowser.selectedBrowser, null, function () {
    is(
      content.document.getElementById("quack").textContent,
      ">o_/",
      '#foo textContent should equal to ">o_/"'
    );
  });
}
