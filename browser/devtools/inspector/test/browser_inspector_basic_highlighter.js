/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  let inspector, doc, toolbox;
  let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
  let {require} = devtools;
  let promise = require("sdk/core/promise");
  let {Task} = Cu.import("resource://gre/modules/Task.jsm", {});

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupTest, content);
  }, true);

  content.location = "data:text/html,<h1>foo</h1><h2>bar</h2>";

  function setupTest() {
    openInspector((aInspector, aToolbox) => {
      toolbox = aToolbox;
      inspector = aInspector;
      inspector.selection.setNode(doc.querySelector("h2"), null);
      inspector.once("inspector-updated", runTests);
    });
  }

  function runTests(aInspector) {
    getHighlighterOutline().setAttribute("disable-transitions", "true");
    Task.spawn(function() {
      yield hoverH1InMarkupView();
      yield assertH1Highlighted();
      yield mouseLeaveMarkupView();
      yield assertNoNodeHighlighted();

      finishUp();
    }).then(null, Cu.reportError);
  }

  function hoverH1InMarkupView() {
    let deferred = promise.defer();

    let container = getContainerForRawNode(inspector.markup, doc.querySelector("h1"));
    EventUtils.synthesizeMouse(container.tagLine, 2, 2, {type: "mousemove"},
      inspector.markup.doc.defaultView);
    inspector.toolbox.once("node-highlight", deferred.resolve);

    return deferred.promise;
  }

  function assertH1Highlighted() {
    ok(isHighlighting(), "The highlighter is shown on a markup container hover");
    is(getHighlitNode(), doc.querySelector("h1"), "The highlighter highlights the right node");
    return promise.resolve();
  }

  function mouseLeaveMarkupView() {
    let deferred = promise.defer();

    // Find another element to mouseover over in order to leave the markup-view
    let btn = toolbox.doc.querySelector(".toolbox-dock-button");

    EventUtils.synthesizeMouse(btn, 2, 2, {type: "mousemove"},
      toolbox.doc.defaultView);
    executeSoon(deferred.resolve);

    return deferred.promise;
  }

  function assertNoNodeHighlighted() {
    ok(!isHighlighting(), "After the mouse left the markup view, the highlighter is hidden");
    return promise.resolve();
  }

  function finishUp() {
    inspector = doc = toolbox = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
