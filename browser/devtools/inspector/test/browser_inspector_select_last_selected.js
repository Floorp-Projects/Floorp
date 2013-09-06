/* -*- Mode: Javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  let inspector, toolbox;
  let page1 = "http://mochi.test:8888/browser/browser/devtools/inspector/test/browser_inspector_select_last_selected.html";
  let page2 = "http://mochi.test:8888/browser/browser/devtools/inspector/test/browser_inspector_select_last_selected2.html";

  waitForExplicitFinish();

  // Create a tab, load test HTML, wait for load and start the tests
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(function() {
      openInspector((aInspector, aToolbox) => {
        inspector = aInspector;
        toolbox = aToolbox;
        startTests();
      });
    }, content);
  }, true);
  content.location = page1;

  function startTests() {
    testSameNodeSelectedOnPageReload();
  }

  function endTests() {
    toolbox.destroy();
    toolbox = inspector = page1 = page2 = null;
    gBrowser.removeCurrentTab();
    finish();
  }

  function testReSelectingAnElement(id, callback) {
    let div = content.document.getElementById(id);
    inspector.selection.setNode(div);
    inspector.once("inspector-updated", () => {
      is(inspector.selection.node, div);
      inspector.once("markuploaded", () => {
        is(inspector.selection.node.id, id, "Node re-selected after reload");
        callback();
      });
      content.location.reload();
    });
  }

  // Test that nodes selected on the test page remain selected after reload
  function testSameNodeSelectedOnPageReload()
  {
    // Select a few nodes and check they are re-selected after reload of the same
    // page
    testReSelectingAnElement("id1", () => {
      testReSelectingAnElement("id2", () => {
        testReSelectingAnElement("id3", () => {
          testReSelectingAnElement("id4", testBodySelectedOnNavigate);
        });
      });
    });
  }

  // Test that since the previously selected node doesn't exist on the new page
  // the body is selected
  function testBodySelectedOnNavigate() {
    // Last node selected was id4, go to a different page and check body is
    // selected
    inspector.once("markuploaded", () => {
      is(
        inspector.selection.node.tagName.toLowerCase(),
        "body",
        "Node not found, selecting body"
      );
      testSameNodeSelectedOnNavigateAwayAndBack();
    });
    content.location = page2;
  }

  // Test that the node selected on page 1 gets selected again after a navigation
  // is made to another page and back again
  function testSameNodeSelectedOnNavigateAwayAndBack() {
    // On page2, select id5
    let id = "id5";
    let div = content.document.getElementById(id);
    inspector.selection.setNode(div);
    inspector.once("inspector-updated", () => {
      is(inspector.selection.node.id, id);
      // go to page1 but do not select anything
      inspector.once("markuploaded", () => {
        // go back to page2 and check id5 is still the current selection
        inspector.once("markuploaded", () => {
          is(inspector.selection.node.id, id, "Node re-selected after navigation");
          endTests();
        });
        content.location = page2;
      });
      content.location = page1;
    });
  }
}
