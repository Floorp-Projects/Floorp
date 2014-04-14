/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  let inspector, doc, toolbox;
  let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
  let {require} = devtools;
  let {Promise: promise} = Cu.import("resource://gre/modules/Promise.jsm", {});
  let {Task} = Cu.import("resource://gre/modules/Task.jsm", {});

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupTest, content);
  }, true);

  content.location = "data:text/html;charset=utf-8,<h1>foo</h1><span>bar</span>";

  function setupTest() {
    openInspector((aInspector, aToolbox) => {
      toolbox = aToolbox;
      inspector = aInspector;
      inspector.selection.setNode(doc.querySelector("span"), "test");
      inspector.toolbox.once("highlighter-ready", runTests);
    });
  }

  function runTests() {
    Task.spawn(function() {
      yield hoverH1InMarkupView();
      yield assertH1Highlighted();

      finishUp();
    }).then(null, Cu.reportError);
  }

  function hoverH1InMarkupView() {
    let deferred = promise.defer();
    let container = getContainerForRawNode(inspector.markup, doc.querySelector("h1"));

    inspector.toolbox.once("highlighter-ready", deferred.resolve);
    EventUtils.synthesizeMouseAtCenter(container.tagLine, {type: "mousemove"},
                                       inspector.markup.doc.defaultView);

    return deferred.promise;
  }

  function assertH1Highlighted() {
    ok(isHighlighting(), "The highlighter is shown on a markup container hover");
    is(getHighlitNode(), doc.querySelector("h1"), "The highlighter highlights the right node");
  }

  function finishUp() {
    inspector = doc = toolbox = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
