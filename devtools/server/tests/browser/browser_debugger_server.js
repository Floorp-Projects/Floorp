/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test basic features of DevToolsServer

add_task(async function () {
  // When running some other tests before, they may not destroy the main server.
  // Do it manually before running our tests.
  if (DevToolsServer.initialized) {
    DevToolsServer.destroy();
  }

  await testDevToolsServerInitialized();
  await testDevToolsServerKeepAlive();
});

async function testDevToolsServerInitialized() {
  const tab = await addTab("data:text/html;charset=utf-8,foo");

  ok(
    !DevToolsServer.initialized,
    "By default, the DevToolsServer isn't initialized in parent process"
  );
  await assertServerInitialized(
    tab,
    false,
    "By default, the DevToolsServer isn't initialized not in content process"
  );
  await assertDevToolsOpened(
    tab,
    false,
    "By default, the DevTools are reported as closed"
  );

  const commands = await CommandsFactory.forTab(tab);

  ok(
    DevToolsServer.initialized,
    "Creating the commands will initialize the DevToolsServer in parent process"
  );
  await assertServerInitialized(
    tab,
    false,
    "Creating the commands isn't enough to initialize the DevToolsServer in content process"
  );
  await assertDevToolsOpened(
    tab,
    false,
    "DevTools are still reported as closed after having created the commands"
  );

  await commands.targetCommand.startListening();

  await assertServerInitialized(
    tab,
    true,
    "Initializing the TargetCommand will initialize the DevToolsServer in content process"
  );
  await assertDevToolsOpened(
    tab,
    true,
    "Initializing the TargetCommand will start reporting the DevTools as opened"
  );

  await commands.destroy();

  // Disconnecting the client will remove all connections from both server, in parent and content process.
  ok(
    !DevToolsServer.initialized,
    "Destroying the commands destroys the DevToolsServer in the parent process"
  );
  await assertServerInitialized(
    tab,
    false,
    "But destroying the commands ends up destroying the DevToolsServer in the content process"
  );
  await assertDevToolsOpened(
    tab,
    false,
    "Destroying the commands will report DevTools as being closed"
  );

  gBrowser.removeCurrentTab();
  DevToolsServer.destroy();
}

async function testDevToolsServerKeepAlive() {
  const tab = await addTab("data:text/html;charset=utf-8,foo");

  await assertServerInitialized(
    tab,
    false,
    "Server not started in content process"
  );
  await assertDevToolsOpened(tab, false, "DevTools are reported as closed");

  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();

  await assertServerInitialized(tab, true, "Server started in content process");
  await assertDevToolsOpened(tab, true, "DevTools are reported as opened");

  info("Set DevToolsServer.keepAlive to true in the content process");
  DevToolsServer.keepAlive = true;
  await setContentServerKeepAlive(tab, true);

  info("Destroy the commands, the content server should be kept alive");
  await commands.destroy();

  await assertServerInitialized(
    tab,
    true,
    "Server still running in content process"
  );
  await assertDevToolsOpened(
    tab,
    false,
    "DevTools are reported as close, even if the server is still running because there is no more client connected"
  );

  ok(
    DevToolsServer.initialized,
    "Destroying the commands never destroys the DevToolsServer in the parent process when keepAlive is true"
  );

  info("Set DevToolsServer.keepAlive back to false");
  DevToolsServer.keepAlive = false;
  await setContentServerKeepAlive(tab, false);

  info("Create and destroy a commands again");
  const newCommands = await CommandsFactory.forTab(tab);
  await newCommands.targetCommand.startListening();

  await newCommands.destroy();

  await assertServerInitialized(
    tab,
    false,
    "Server stopped in content process"
  );
  await assertDevToolsOpened(
    tab,
    false,
    "DevTools are reported as closed after destroying the second commands"
  );

  ok(
    !DevToolsServer.initialized,
    "When turning keepAlive to false, the server in the parent process is destroyed"
  );

  gBrowser.removeCurrentTab();
  DevToolsServer.destroy();
}

async function assertServerInitialized(tab, expected, message) {
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [expected, message],
    function (_expected, _message) {
      const { require } = ChromeUtils.importESModule(
        "resource://devtools/shared/loader/Loader.sys.mjs"
      );
      const {
        DevToolsServer,
      } = require("resource://devtools/server/devtools-server.js");
      is(DevToolsServer.initialized, _expected, _message);
    }
  );
}

async function assertDevToolsOpened(tab, expected, message) {
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [expected, message],
    function (_expected, _message) {
      is(ChromeUtils.isDevToolsOpened(), _expected, _message);
    }
  );
}

async function setContentServerKeepAlive(tab, keepAlive) {
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [keepAlive],
    function (_keepAlive) {
      const { require } = ChromeUtils.importESModule(
        "resource://devtools/shared/loader/Loader.sys.mjs"
      );
      const {
        DevToolsServer,
      } = require("resource://devtools/server/devtools-server.js");
      DevToolsServer.keepAlive = _keepAlive;
    }
  );
}
