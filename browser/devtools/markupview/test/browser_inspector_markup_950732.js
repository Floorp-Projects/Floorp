/* Any copyright", " is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the markup view loads only as many nodes as specified
 * by the devtools.markup.pagesize preference.
 */

let {devtools} = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});
let promise = devtools.require("sdk/core/promise");
let {Task} = Cu.import("resource://gre/modules/Task.jsm", {});

// Make sure nodes are hidden when there are more than 5 in a row
registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.markup.pagesize");
});
Services.prefs.setIntPref("devtools.markup.pagesize", 5);

function test() {
  waitForExplicitFinish();

  let doc;
  let inspector;
  let markup;

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(runTests, content);
  }, true);
  content.location = "http://mochi.test:8888/browser/browser/devtools/markupview/test/browser_inspector_markup_950732.html";

  function runTests() {
    Task.spawn(function() {
      yield openMarkupView();
      yield selectUL();
      yield reloadPage();
      yield showAllNodes();

      assertAllNodesAreVisible();
      finishUp();
    }).then(null, Cu.reportError);
  }

  function openMarkupView() {
    let deferred = promise.defer();

    var target = TargetFactory.forTab(gBrowser.selectedTab);
    let toolbox = gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
      inspector = toolbox.getCurrentPanel();
      markup = inspector.markup;
      inspector.once("inspector-updated", deferred.resolve);
    });

    return deferred.promise;
  }

  function selectUL() {
    let deferred = promise.defer();

    let container = getContainerForRawNode(markup, doc.querySelector("ul"));
    let win = container.elt.ownerDocument.defaultView;

    EventUtils.sendMouseEvent({type: "mousedown"}, container.elt, win);
    inspector.once("inspector-updated", deferred.resolve);

    return deferred.promise;
  }

  function reloadPage() {
    let deferred = promise.defer();

    inspector.once("new-root", () => {
      doc = content.document;
      markup = inspector.markup;
      markup._waitForChildren().then(deferred.resolve);
    });
    content.location.reload();

    return deferred.promise;
  }

  function showAllNodes() {
    let container = getContainerForRawNode(markup, doc.querySelector("ul"));
    let button = container.elt.querySelector("button");
    let win = button.ownerDocument.defaultView;

    EventUtils.sendMouseEvent({type: "click"}, button, win);
    return markup._waitForChildren();
  }

  function assertAllNodesAreVisible() {
    let ul = doc.querySelector("ul");
    let container = getContainerForRawNode(markup, ul);
    ok(!container.elt.querySelector("button"), "All nodes button isn't here");
    is(container.children.childNodes.length, ul.children.length);
  }

  function finishUp() {
    doc = inspector = markup = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
