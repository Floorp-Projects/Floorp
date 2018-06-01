/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webide/test/doc_tabs.html";

function test() {
  waitForExplicitFinish();
  SimpleTest.requestCompleteLog();

  (async function() {
    // Since we test the connections set below, destroy the server in case it
    // was left open.
    DebuggerServer.destroy();
    DebuggerServer.init();
    DebuggerServer.registerAllActors();

    let tab = await addTab(TEST_URI);

    const win = await openWebIDE();
    const docProject = getProjectDocument(win);
    const docRuntime = getRuntimeDocument(win);

    await connectToLocal(win, docRuntime);

    is(Object.keys(DebuggerServer._connections).length, 1, "Locally connected");

    await selectTabProject(win, docProject);

    ok(win.UI.toolboxPromise, "Toolbox promise exists");
    await win.UI.toolboxPromise;

    const project = win.AppManager.selectedProject;
    is(project.location, TEST_URI, "Location is correct");
    is(project.name, "example.com: Test Tab", "Name is correct");

    // Ensure tab list changes are noticed
    const tabsNode = docProject.querySelector("#project-panel-tabs");
    is(tabsNode.querySelectorAll(".panel-item").length, 2, "2 tabs available");
    await removeTab(tab);
    await waitForUpdate(win, "project");
    await waitForUpdate(win, "runtime-targets");
    is(tabsNode.querySelectorAll(".panel-item").length, 1, "1 tab available");

    tab = await addTab(TEST_URI);

    is(tabsNode.querySelectorAll(".panel-item").length, 2, "2 tabs available");

    await removeTab(tab);

    is(tabsNode.querySelectorAll(".panel-item").length, 2, "2 tabs available");

    docProject.querySelector("#refresh-tabs").click();

    await waitForUpdate(win, "runtime-targets");

    is(tabsNode.querySelectorAll(".panel-item").length, 1, "1 tab available");

    await win.Cmds.disconnectRuntime();
    await closeWebIDE(win);

    DebuggerServer.destroy();
  })().then(finish, handleError);
}

function connectToLocal(win, docRuntime) {
  return new Promise(resolve => {
    win.AppManager.connection.once(
      win.Connection.Events.CONNECTED,
      resolve);
    docRuntime.querySelectorAll(".runtime-panel-item-other")[1].click();
  });
}

function selectTabProject(win, docProject) {
  return (async function() {
    await waitForUpdate(win, "runtime-targets");
    const tabsNode = docProject.querySelector("#project-panel-tabs");
    const tabNode = tabsNode.querySelectorAll(".panel-item")[1];
    const project = waitForUpdate(win, "project");
    tabNode.click();
    await project;
  })();
}
