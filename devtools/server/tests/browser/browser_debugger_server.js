/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
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
});

async function testDebuggerServerInitialized() {
  const browser = await addTab("data:text/html;charset=utf-8,foo");
  const tab = gBrowser.getTabForBrowser(browser);

  ok(!DebuggerServer.initialized,
    "By default, the DebuggerServer isn't initialized in parent process");
  await ContentTask.spawn(browser, null, function() {
    const {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
    const {DebuggerServer} = require("devtools/server/main");
    ok(!DebuggerServer.initialized,
      "By default, the DebuggerServer isn't initialized not in content process");
  });

  const target = await TargetFactory.forTab(tab);

  ok(DebuggerServer.initialized,
    "TargetFactory.forTab will initialize the DebuggerServer in parent process");
  await ContentTask.spawn(browser, null, function() {
    const {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
    const {DebuggerServer} = require("devtools/server/main");
    ok(DebuggerServer.initialized,
      "TargetFactory.forTab will initialize the DebuggerServer in content process");
  });

  await target.destroy();

  // Disconnecting the client will remove all connections from both server,
  // in parent and content process. But only the one in the content process will be
  // destroyed.
  ok(DebuggerServer.initialized,
    "Destroying the target doesn't destroy the DebuggerServer in the parent process");
  await ContentTask.spawn(browser, null, function() {
    const {require} = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
    const {DebuggerServer} = require("devtools/server/main");
    ok(!DebuggerServer.initialized,
      "But destroying the target ends up destroying the DebuggerServer in the content" +
      " process");
  });

  gBrowser.removeCurrentTab();
}
