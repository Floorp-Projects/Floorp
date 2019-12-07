/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test basic features of DebuggerServer

add_task(async function() {
  // When running some other tests before, they may not destroy the main server.
  // Do it manually before running our tests.
  if (DebuggerServer.initialized) {
    DebuggerServer.destroy();
  }

  await testDebuggerServerInitialized();
  await testDebuggerServerKeepAlive();
});

async function testDebuggerServerInitialized() {
  const browser = await addTab("data:text/html;charset=utf-8,foo");
  const tab = gBrowser.getTabForBrowser(browser);

  ok(
    !DebuggerServer.initialized,
    "By default, the DebuggerServer isn't initialized in parent process"
  );
  await assertServerInitialized(
    browser,
    false,
    "By default, the DebuggerServer isn't initialized not in content process"
  );

  const target = await TargetFactory.forTab(tab);

  ok(
    DebuggerServer.initialized,
    "TargetFactory.forTab will initialize the DebuggerServer in parent process"
  );
  await assertServerInitialized(
    browser,
    true,
    "TargetFactory.forTab will initialize the DebuggerServer in content process"
  );

  await target.destroy();

  // Disconnecting the client will remove all connections from both server,
  // in parent and content process. But only the one in the content process will be
  // destroyed.
  ok(
    DebuggerServer.initialized,
    "Destroying the target doesn't destroy the DebuggerServer in the parent process"
  );
  await assertServerInitialized(
    browser,
    false,
    "But destroying the target ends up destroying the DebuggerServer in the content" +
      " process"
  );

  gBrowser.removeCurrentTab();
  DebuggerServer.destroy();
}

async function testDebuggerServerKeepAlive() {
  const browser = await addTab("data:text/html;charset=utf-8,foo");
  const tab = gBrowser.getTabForBrowser(browser);

  await assertServerInitialized(
    browser,
    false,
    "Server not started in content process"
  );

  const target = await TargetFactory.forTab(tab);
  await assertServerInitialized(
    browser,
    true,
    "Server started in content process"
  );

  info("Set DebuggerServer.keepAlive to true in the content process");
  await setContentServerKeepAlive(browser, true);

  info("Destroy the target, the content server should be kept alive");
  await target.destroy();

  await assertServerInitialized(
    browser,
    true,
    "Server still running in content process"
  );

  info("Set DebuggerServer.keepAlive back to false");
  await setContentServerKeepAlive(browser, false);

  info("Create and destroy a target again");
  const newTarget = await TargetFactory.forTab(tab);
  await newTarget.destroy();

  await assertServerInitialized(
    browser,
    false,
    "Server stopped in content process"
  );

  gBrowser.removeCurrentTab();
  DebuggerServer.destroy();
}

async function assertServerInitialized(browser, expected, message) {
  const isInitialized = await ContentTask.spawn(browser, null, function() {
    const { require } = ChromeUtils.import(
      "resource://devtools/shared/Loader.jsm"
    );
    const { DebuggerServer } = require("devtools/server/debugger-server");
    return DebuggerServer.initialized;
  });
  is(isInitialized, expected, message);
}

async function setContentServerKeepAlive(browser, keepAlive, message) {
  await ContentTask.spawn(browser, keepAlive, function(_keepAlive) {
    const { require } = ChromeUtils.import(
      "resource://devtools/shared/Loader.jsm"
    );
    const { DebuggerServer } = require("devtools/server/debugger-server");
    DebuggerServer.keepAlive = _keepAlive;
  });
}
