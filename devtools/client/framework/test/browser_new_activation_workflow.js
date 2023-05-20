/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests devtools API

var toolbox;

function test() {
  addTab("about:blank").then(async function () {
    loadWebConsole().then(function () {
      console.log("loaded");
    });
  });
}

function loadWebConsole() {
  ok(gDevTools, "gDevTools exists");
  const tab = gBrowser.selectedTab;
  return gDevTools
    .showToolboxForTab(tab, { toolId: "webconsole" })
    .then(function (aToolbox) {
      toolbox = aToolbox;
      checkToolLoading();
    });
}

function checkToolLoading() {
  is(toolbox.currentToolId, "webconsole", "The web console is selected");
  ok(toolbox.isReady, "toolbox is ready");

  selectAndCheckById("jsdebugger").then(function () {
    selectAndCheckById("styleeditor").then(function () {
      testToggle();
    });
  });
}

function selectAndCheckById(id) {
  return toolbox.selectTool(id).then(function () {
    const tab = toolbox.doc.getElementById("toolbox-tab-" + id);
    is(
      tab.classList.contains("selected"),
      true,
      "The " + id + " tab is selected"
    );
    is(
      tab.getAttribute("aria-pressed"),
      "true",
      "The " + id + " tab is pressed"
    );
  });
}

function testToggle() {
  toolbox.once("destroyed", async () => {
    // Cannot reuse a target after it's destroyed.
    gDevTools
      .showToolboxForTab(gBrowser.selectedTab, { toolId: "styleeditor" })
      .then(function (aToolbox) {
        toolbox = aToolbox;
        is(
          toolbox.currentToolId,
          "styleeditor",
          "The style editor is selected"
        );
        finishUp();
      });
  });

  toolbox.destroy();
}

function finishUp() {
  toolbox.destroy().then(function () {
    toolbox = null;
    gBrowser.removeCurrentTab();
    finish();
  });
}
