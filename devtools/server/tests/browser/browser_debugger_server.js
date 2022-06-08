/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test basic features of DevToolsServer

add_task(async function() {
  // When running some other tests before, they may not destroy the main server.
  // Do it manually before running our tests.
  if (DevToolsServer.initialized) {
    DevToolsServer.destroy();
  }

  await testDevToolsServerInitialized();
  await testDevToolsServerKeepAlive();
});

async function testDevToolsServerInitialized() {
  const browser = await addTab("data:text/html;charset=utf-8,foo");
  const tab = gBrowser.getTabForBrowser(browser);

  ok(
    !DevToolsServer.initialized,
    "By default, the DevToolsServer isn't initialized in parent process"
  );
  await assertServerInitialized(
    browser,
    false,
    "By default, the DevToolsServer isn't initialized not in content process"
  );

  const commands = await CommandsFactory.forTab(tab);

  ok(
    DevToolsServer.initialized,
    "Creating the commands will initialize the DevToolsServer in parent process"
  );
  await assertServerInitialized(
    browser,
    false,
    "Creating the commands isn't enough to initialize the DevToolsServer in content process"
  );

  await commands.targetCommand.startListening();

  await assertServerInitialized(
    browser,
    true,
    "Initializing the TargetCommand will initialize the DevToolsServer in content process"
  );

  await commands.destroy();

  // Disconnecting the client will remove all connections from both server, in parent and content process.
  ok(
    !DevToolsServer.initialized,
    "Destroying the commands destroys the DevToolsServer in the parent process"
  );
  await assertServerInitialized(
    browser,
    false,
    "But destroying the commands ends up destroying the DevToolsServer in the content process"
  );

  gBrowser.removeCurrentTab();
  DevToolsServer.destroy();
}

async function testDevToolsServerKeepAlive() {
  const browser = await addTab("data:text/html;charset=utf-8,foo");
  const tab = gBrowser.getTabForBrowser(browser);

  await assertServerInitialized(
    browser,
    false,
    "Server not started in content process"
  );

  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();

  await assertServerInitialized(
    browser,
    true,
    "Server started in content process"
  );

  info("Set DevToolsServer.keepAlive to true in the content process");
  DevToolsServer.keepAlive = true;
  await setContentServerKeepAlive(browser, true);

  info("Destroy the commands, the content server should be kept alive");
  await commands.destroy();

  await assertServerInitialized(
    browser,
    true,
    "Server still running in content process"
  );

  ok(
    DevToolsServer.initialized,
    "Destroying the commands never destroys the DevToolsServer in the parent process when keepAlive is true"
  );

  info("Set DevToolsServer.keepAlive back to false");
  DevToolsServer.keepAlive = false;
  await setContentServerKeepAlive(browser, false);

  info("Create and destroy a commands again");
  const newCommands = await CommandsFactory.forTab(tab);
  await newCommands.targetCommand.startListening();

  await newCommands.destroy();

  await assertServerInitialized(
    browser,
    false,
    "Server stopped in content process"
  );

  ok(
    !DevToolsServer.initialized,
    "When turning keepAlive to false, the server in the parent process is destroyed"
  );

  gBrowser.removeCurrentTab();
  DevToolsServer.destroy();
}

async function assertServerInitialized(browser, expected, message) {
  const isInitialized = await SpecialPowers.spawn(browser, [], function() {
    const { require } = ChromeUtils.import(
      "resource://devtools/shared/loader/Loader.jsm"
    );
    const { DevToolsServer } = require("devtools/server/devtools-server");
    return DevToolsServer.initialized;
  });
  is(isInitialized, expected, message);
}

async function setContentServerKeepAlive(browser, keepAlive, message) {
  await SpecialPowers.spawn(browser, [keepAlive], function(_keepAlive) {
    const { require } = ChromeUtils.import(
      "resource://devtools/shared/loader/Loader.jsm"
    );
    const { DevToolsServer } = require("devtools/server/devtools-server");
    DevToolsServer.keepAlive = _keepAlive;
  });
}
