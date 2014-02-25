/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that searching for nodes using the selector-search input expands and
// selects the right nodes in the markup-view, even when those nodes are deeply
// nested (and therefore not attached yet when the markup-view is initialized).

const TEST_URL = "http://mochi.test:8888/browser/browser/devtools/markupview/test/browser_inspector_markup_962647_search.html";

function test() {
  waitForExplicitFinish();

  let p = content.document.querySelector("p");
  Task.spawn(function() {
    info("loading the test page");
    yield addTab(TEST_URL);

    info("opening the inspector");
    let {inspector, toolbox} = yield openInspector();

    ok(!getContainerForRawNode(inspector.markup, getNode("em")),
      "The <em> tag isn't present yet in the markup-view");

    // Searching for the innermost element first makes sure that the inspector
    // back-end is able to attach the resulting node to the tree it knows at the
    // moment. When the inspector is started, the <body> is the default selected
    // node, and only the parents up to the ROOT are known, and its direct children
    info("searching for the innermost child: <em>");
    let updated = inspector.once("inspector-updated");
    searchUsingSelectorSearch("em", inspector);
    yield updated;

    ok(getContainerForRawNode(inspector.markup, getNode("em")),
      "The <em> tag is now imported in the markup-view");
    is(inspector.selection.node, getNode("em"),
      "The <em> tag is the currently selected node");

    info("searching for other nodes too");
    for (let node of ["span", "li", "ul"]) {
      let updated = inspector.once("inspector-updated");
      searchUsingSelectorSearch(node, inspector);
      yield updated;
      is(inspector.selection.node, getNode(node),
        "The <" + node + "> tag is the currently selected node");
    }

    gBrowser.removeCurrentTab();
  }).then(null, ok.bind(null, false)).then(finish);
}
