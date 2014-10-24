/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const TEST_URI = "http://example.com/browser/browser/devtools/webide/test/doc_tabs.html";

function test() {
  waitForExplicitFinish();
  SimpleTest.requestCompleteLog();

  Task.spawn(function() {
    const { DebuggerServer } =
      Cu.import("resource://gre/modules/devtools/dbg-server.jsm", {});

    // Since we test the connections set below, destroy the server in case it
    // was left open.
    DebuggerServer.destroy();
    DebuggerServer.init(function () { return true; });
    DebuggerServer.addBrowserActors();

    let tab = yield addTab(TEST_URI);

    let win = yield openWebIDE();

    yield connectToLocal(win);

    is(Object.keys(DebuggerServer._connections).length, 1, "Locally connected");

    yield selectTabProject(win);

    let project = win.AppManager.selectedProject;
    is(project.location, TEST_URI, "Location is correct");
    is(project.name, "example.com: Test Tab", "Name is correct");

    yield closeWebIDE(win);
    DebuggerServer.destroy();
    yield removeTab(tab);
  }).then(finish, handleError);
}

function connectToLocal(win) {
  let deferred = promise.defer();
  win.AppManager.connection.once(
      win.Connection.Events.CONNECTED,
      () => deferred.resolve());
  win.document.querySelectorAll(".runtime-panel-item-other")[1].click();
  return deferred.promise;
}

function selectTabProject(win) {
  return Task.spawn(function() {
    yield win.AppManager.listTabs();
    win.Cmds.showProjectPanel();
    yield nextTick();
    let tabsNode = win.document.querySelector("#project-panel-tabs");
    let tabNode = tabsNode.querySelectorAll(".panel-item")[1];
    tabNode.click();
  });
}
