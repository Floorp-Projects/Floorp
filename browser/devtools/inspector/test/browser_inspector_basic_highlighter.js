/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


function test() {
  let inspector, doc;
  let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
  let {require} = devtools;
  let promise = require("sdk/core/promise");
  let { Task } = Cu.import("resource://gre/modules/Task.jsm", {});

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupTest, content);
  }, true);

  content.location = "data:text/html,<h1>foo<h1><h2>bar</h2>";

  function setupTest() {
    let h = require("devtools/inspector/highlighter");
    h._forceBasic.value = true;
    openInspector(runTests);
  }

  function runTests(aInspector) {
    inspector = aInspector;

    Task.spawn(function() {
      yield selectH1();
      yield verifyH1Selected();
      yield deselect();
      yield verifyNoNodeSelected();

      yield selectH1();
      yield verifyH1Selected();
      yield destroyInspector();
      yield verifyNoNodeSelected();

      finishUp();
    }).then(null, Cu.reportError);
  }

  function selectH1() {
    let deferred = promise.defer();
    let h1 = doc.querySelector("h1");
    inspector.selection.once("new-node-front", () => {
      executeSoon(deferred.resolve);
    });
    inspector.selection.setNode(h1);
    return deferred.promise;
  }

  function verifyH1Selected() {
    let h1 = doc.querySelector("h1");
    let nodes = doc.querySelectorAll(":-moz-devtools-highlighted");
    is(nodes.length, 1, "only one node selected");
    is(nodes[0], h1, "h1 selected");
    return promise.resolve();
  }

  function deselect() {
    let deferred = promise.defer();
    inspector.selection.once("new-node-front", () => {
      executeSoon(deferred.resolve);
    });
    inspector.selection.setNode(null);
    return deferred.promise;
  }

  function destroyInspector() {
    return inspector.destroy();
  }

  function verifyNoNodeSelected() {
    is(doc.querySelectorAll(":-moz-devtools-highlighted").length, 0, "no node selected");
    return promise.resolve();
  }

  function finishUp() {
    let h = require("devtools/inspector/highlighter");
    h._forceBasic.value = false;
    inspector = doc = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}


