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
    inspector.destroy().then(() =>
      toolbox.destroy()
    ).then(() => {
      toolbox = inspector = page1 = page2 = null;
      gBrowser.removeCurrentTab();
      finish();
    });
  }

  function loadPageAnd(page, callback) {
    inspector.once("markuploaded", () => {
      callback();
    });

    if (page) {
      content.location = page;
    } else {
      content.location.reload();
    }
  }

  function reloadAndReselect(id, callback) {
    let div = content.document.getElementById(id);

    inspector.once("inspector-updated", () => {
      is(inspector.selection.node, div);

      loadPageAnd(false, () => {
        is(inspector.selection.node.id, id, "Node re-selected after reload");
        callback();
      });
    });

    inspector.selection.setNode(div);
  }

  // Test that nodes selected on the test page remain selected after reload
  function testSameNodeSelectedOnPageReload()
  {
    // Select a few nodes and check they are re-selected after reload of the same
    // page
    reloadAndReselect("id1", () => {
      reloadAndReselect("id2", () => {
        reloadAndReselect("id3", () => {
          reloadAndReselect("id4", testBodySelectedOnNavigate);
        });
      });
    });
  }

  // Test that since the previously selected node doesn't exist on the new page
  // the body is selected
  function testBodySelectedOnNavigate() {
    // Last node selected was id4, go to a different page and check body is
    // selected
    loadPageAnd(page2, () => {
      is(
        inspector.selection.node.tagName.toLowerCase(),
        "body",
        "Node not found, body selected"
      );
      testSameNodeSelectedOnNavigateAwayAndBack();
    });
  }

  // Test that the node selected on page 1 gets selected again after a navigation
  // is made to another page and back again
  function testSameNodeSelectedOnNavigateAwayAndBack() {
    // On page2, select id5
    let id = "id5";
    let div = content.document.getElementById(id);

    inspector.once("inspector-updated", () => {
      is(inspector.selection.node.id, id);

      // go to page1 but do not select anything
      loadPageAnd(page1, () => {
          // go back to page2 and check id5 is still the current selection
        loadPageAnd(page2, () => {
          is(inspector.selection.node.id, id, "Node re-selected after navigation");
          endTests();
        });
      });
    });

    inspector.selection.setNode(div);
  }
}
