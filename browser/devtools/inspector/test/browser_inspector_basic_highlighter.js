/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


function test()
{
  let inspector, doc;
  let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
  let {require} = devtools;

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupTest, content);
  }, true);

  content.location = "data:text/html,<h1>foo<h1><h2>bar</h2>";

  function setupTest()
  {
    let h = require("devtools/inspector/highlighter");
    h._forceBasic.value = true;
    openInspector(runTests);
  }

  function runTests(aInspector)
  {
    inspector = aInspector;
    let h1 = doc.querySelector("h1");
    inspector.selection.once("new-node-front", () => executeSoon(testH1Selected));
    inspector.selection.setNode(h1);
  }

  function testH1Selected() {
    let h1 = doc.querySelector("h1");
    let nodes = doc.querySelectorAll(":-moz-devtools-highlighted");
    is(nodes.length, 1, "only one node selected");
    is(nodes[0], h1, "h1 selected");
    inspector.selection.once("new-node-front", () => executeSoon(testNoNodeSelected));
    inspector.selection.setNode(null);
  }

  function testNoNodeSelected() {
    ok(doc.querySelectorAll(":-moz-devtools-highlighted").length === 0, "no node selected");
    finishUp();
  }

  function finishUp() {
    let h = require("devtools/inspector/highlighter");
    h._forceBasic.value = false;
    gBrowser.removeCurrentTab();
    finish();
  }
}


