/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Make sure the listTabs request works as specified.
 */

var { DebuggerServer } = require("devtools/server/main");
var { DebuggerClient } = require("devtools/shared/client/debugger-client");
var { Task } = require("devtools/shared/task");

const TAB1_URL = EXAMPLE_URL + "doc_empty-tab-01.html";

var gClient;

function test() {
  DebuggerServer.init();
  DebuggerServer.registerAllActors();

  const transport = DebuggerServer.connectPipe();
  gClient = new DebuggerClient(transport);
  gClient.connect().then(Task.async(function* ([aType, aTraits]) {
    is(aType, "browser",
      "Root actor should identify itself as a browser.");
    const tab = yield addTab(TAB1_URL);

    let { tabs } = yield gClient.listTabs();
    is(tabs.length, 2, "Should be two tabs");
    const tabGrip = tabs.filter(a => a.url == TAB1_URL).pop();
    ok(tabGrip, "Should have an actor for the tab");

    let response = yield gClient.request({ to: tabGrip.actor, type: "attach" });
    is(response.type, "tabAttached", "Should have attached");

    response = yield gClient.listTabs();
    tabs = response.tabs;

    response = yield gClient.request({ to: tabGrip.actor, type: "detach" });
    is(response.type, "detached", "Should have detached");

    const newGrip = tabs.filter(a => a.url == TAB1_URL).pop();
    is(newGrip.actor, tabGrip.actor, "Should have the same actor for the same tab");

    response = yield gClient.request({ to: tabGrip.actor, type: "attach" });
    is(response.type, "tabAttached", "Should have attached");
    response = yield gClient.request({ to: tabGrip.actor, type: "detach" });
    is(response.type, "detached", "Should have detached");

    yield removeTab(tab);
    yield gClient.close();
    finish();
  }));
}

registerCleanupFunction(function() {
  gClient = null;
});
