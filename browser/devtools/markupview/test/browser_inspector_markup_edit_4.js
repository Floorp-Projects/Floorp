/* Any copyright", " is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that a node can be deleted from the markup-view with the delete key

waitForExplicitFinish();

const TEST_URL = "data:text/html,<div id='delete-me'></div>";

function test() {
  Task.spawn(function() {
    info("Opening the inspector on the test page");
    let {toolbox, inspector} = yield addTab(TEST_URL).then(openInspector);

    info("Selecting the test node by clicking on it to make sure it receives focus");
    let node = content.document.getElementById("delete-me");
    yield clickContainer(node, inspector);

    info("Deleting the element with the keyboard");
    let mutated = inspector.once("markupmutation");
    EventUtils.sendKey("delete", inspector.panelWin);
    yield mutated;

    info("Checking that it's gone, baby gone!");
    ok(!content.document.getElementById("delete-me"), "The test node does not exist");

    yield undoChange(inspector);
    ok(content.document.getElementById("delete-me"), "The test node is back!");

    yield inspector.once("inspector-updated");

    gBrowser.removeCurrentTab();
  }).then(null, ok.bind(null, false)).then(finish);
}
