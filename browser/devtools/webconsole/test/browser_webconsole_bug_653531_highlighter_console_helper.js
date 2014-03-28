/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests that the $0 console helper works as intended.

let inspector, h1;

function createDocument() {
  let doc = content.document;
  let div = doc.createElement("div");
  h1 = doc.createElement("h1");
  let p1 = doc.createElement("p");
  let p2 = doc.createElement("p");
  let div2 = doc.createElement("div");
  let p3 = doc.createElement("p");
  doc.title = "Inspector Tree Selection Test";
  h1.textContent = "Inspector Tree Selection Test";
  p1.textContent = "This is some example text";
  p2.textContent = "Lorem ipsum dolor sit amet, consectetur adipisicing " +
    "elit, sed do eiusmod tempor incididunt ut labore et dolore magna " +
    "aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco " +
    "laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure " +
    "dolor in reprehenderit in voluptate velit esse cillum dolore eu " +
    "fugiat nulla pariatur. Excepteur sint occaecat cupidatat non " +
    "proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
  p3.textContent = "Lorem ipsum dolor sit amet, consectetur adipisicing " +
    "elit, sed do eiusmod tempor incididunt ut labore et dolore magna " +
    "aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco " +
    "laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure " +
    "dolor in reprehenderit in voluptate velit esse cillum dolore eu " +
    "fugiat nulla pariatur. Excepteur sint occaecat cupidatat non " +
    "proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
  div.appendChild(h1);
  div.appendChild(p1);
  div.appendChild(p2);
  div2.appendChild(p3);
  doc.body.appendChild(div);
  doc.body.appendChild(div2);
  setupHighlighterTests();
}

function setupHighlighterTests() {
  ok(h1, "we have the header node");
  openInspector(runSelectionTests);
}

function runSelectionTests(aInspector) {
  inspector = aInspector;

  inspector.toolbox.highlighterUtils.startPicker();
  inspector.toolbox.once("picker-started", () => {
    info("Picker mode started, now clicking on H1 to select that node");
    executeSoon(() => {
      h1.scrollIntoView();
      EventUtils.synthesizeMouseAtCenter(h1, {}, content);
      inspector.toolbox.once("picker-stopped", () => {
        info("Picker mode stopped, H1 selected, now switching to the console");
        openConsole(gBrowser.selectedTab).then(performWebConsoleTests);
      });
    });
  });
}

function performWebConsoleTests(hud) {
  let target = TargetFactory.forTab(gBrowser.selectedTab);
  let jsterm = hud.jsterm;
  outputNode = hud.outputNode;

  jsterm.clearOutput();
  jsterm.execute("$0", onNodeOutput);

  function onNodeOutput(node) {
    isnot(node.textContent.indexOf("<h1>"), -1, "correct output for $0");

    jsterm.clearOutput();
    jsterm.execute("$0.textContent = 'bug653531'", onNodeUpdate);
  }

  function onNodeUpdate(node) {
    isnot(node.textContent.indexOf("bug653531"), -1,
          "correct output for $0.textContent");
    is(inspector.selection.node.textContent, "bug653531",
       "node successfully updated");

    inspector = h1 = null;
    gBrowser.removeCurrentTab();
    finishTest();
  }
}

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
    waitForFocus(createDocument, content);
  }, true);

  content.location = "data:text/html;charset=utf-8,test for highlighter helper in web console";
}
