/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that the breadcrumbs widget refreshes correctly when there are markup
// mutations (and that it doesn't refresh when those mutations don't change its
// output).

const TEST_URI = URL_ROOT + "doc_inspector_breadcrumbs.html";

// Each item in the TEST_DATA array is a test case that should contain the
// following properties:
// - desc {String} A description of this test case (will be logged).
// - setup {Function*} A generator function (can yield promises) that sets up
//   the test case. Useful for selecting a node before starting the test.
// - run {Function*} A generator function (can yield promises) that runs the
//   actual test case, i.e, mutates the content DOM to cause the breadcrumbs
//   to refresh, or not.
// - shouldRefresh {Boolean} Once the `run` function has completed, and the test
//   has detected that the page has changed, this boolean instructs the test to
//   verify if the breadcrumbs has refreshed or not.
// - output {Array} A list of strings for the text that should be found in each
//   button after the test has run.
const TEST_DATA = [{
  desc: "Adding a child at the end of the chain shouldn't change anything",
  setup: function* (inspector) {
    yield selectNode("#i1111", inspector);
  },
  run: function* ({walker, selection}) {
    yield walker.setInnerHTML(selection.nodeFront, "<b>test</b>");
  },
  shouldRefresh: false,
  output: ["html", "body", "article#i1", "div#i11", "div#i111", "div#i1111"]
}, {
  desc: "Updating an ID to an displayed element should refresh",
  setup: function* () {},
  run: function* ({walker}) {
    let node = yield walker.querySelector(walker.rootNode, "#i1");
    yield node.modifyAttributes([{
      attributeName: "id",
      newValue: "i1-changed"
    }]);
  },
  shouldRefresh: true,
  output: ["html", "body", "article#i1-changed", "div#i11", "div#i111",
           "div#i1111"]
}, {
  desc: "Updating an class to a displayed element should refresh",
  setup: function* () {},
  run: function* ({walker}) {
    let node = yield walker.querySelector(walker.rootNode, "body");
    yield node.modifyAttributes([{
      attributeName: "class",
      newValue: "test-class"
    }]);
  },
  shouldRefresh: true,
  output: ["html", "body.test-class", "article#i1-changed", "div#i11",
           "div#i111", "div#i1111"]
}, {
  desc: "Updating a non id/class attribute to a displayed element should not " +
        "refresh",
  setup: function* () {},
  run: function* ({walker}) {
    let node = yield walker.querySelector(walker.rootNode, "#i11");
    yield node.modifyAttributes([{
      attributeName: "name",
      newValue: "value"
    }]);
  },
  shouldRefresh: false,
  output: ["html", "body.test-class", "article#i1-changed", "div#i11",
           "div#i111", "div#i1111"]
}, {
  desc: "Moving a child in an element that's not displayed should not refresh",
  setup: function* () {},
  run: function* ({walker}) {
    // Re-append #i1211 as a last child of #i2.
    let parent = yield walker.querySelector(walker.rootNode, "#i2");
    let child = yield walker.querySelector(walker.rootNode, "#i211");
    yield walker.insertBefore(child, parent);
  },
  shouldRefresh: false,
  output: ["html", "body.test-class", "article#i1-changed", "div#i11",
           "div#i111", "div#i1111"]
}, {
  desc: "Moving an undisplayed child in a displayed element should not refresh",
  setup: function* () {},
  run: function* ({walker}) {
    // Re-append #i2 in body (move it to the end).
    let parent = yield walker.querySelector(walker.rootNode, "body");
    let child = yield walker.querySelector(walker.rootNode, "#i2");
    yield walker.insertBefore(child, parent);
  },
  shouldRefresh: false,
  output: ["html", "body.test-class", "article#i1-changed", "div#i11",
           "div#i111", "div#i1111"]
}, {
  desc: "Updating attributes on an element that's not displayed should not " +
        "refresh",
  setup: function* () {},
  run: function* ({walker}) {
    let node = yield walker.querySelector(walker.rootNode, "#i2");
    yield node.modifyAttributes([{
      attributeName: "id",
      newValue: "i2-changed"
    }, {
      attributeName: "class",
      newValue: "test-class"
    }]);
  },
  shouldRefresh: false,
  output: ["html", "body.test-class", "article#i1-changed", "div#i11",
           "div#i111", "div#i1111"]
}, {
  desc: "Removing the currently selected node should refresh",
  setup: function* (inspector) {
    yield selectNode("#i2-changed", inspector);
  },
  run: function* ({walker, selection}) {
    yield walker.removeNode(selection.nodeFront);
  },
  shouldRefresh: true,
  output: ["html", "body.test-class"]
}, {
  desc: "Changing the class of the currently selected node should refresh",
  setup: function* () {},
  run: function* ({selection}) {
    yield selection.nodeFront.modifyAttributes([{
      attributeName: "class",
      newValue: "test-class-changed"
    }]);
  },
  shouldRefresh: true,
  output: ["html", "body.test-class-changed"]
}, {
  desc: "Changing the id of the currently selected node should refresh",
  setup: function* () {},
  run: function* ({selection}) {
    yield selection.nodeFront.modifyAttributes([{
      attributeName: "id",
      newValue: "new-id"
    }]);
  },
  shouldRefresh: true,
  output: ["html", "body#new-id.test-class-changed"]
}];

add_task(function* () {
  let {inspector} = yield openInspectorForURL(TEST_URI);
  let container = inspector.panelDoc.getElementById("inspector-breadcrumbs");
  let win = container.ownerDocument.defaultView;

  for (let {desc, setup, run, shouldRefresh, output} of TEST_DATA) {
    info("Running test case: " + desc);

    info("Listen to markupmutation events from the inspector to know when a " +
         "test case has completed");
    let onContentMutation = inspector.once("markupmutation");

    info("Running setup");
    yield setup(inspector);

    info("Listen to mutations on the breadcrumbs container");
    let hasBreadcrumbsMutated = false;
    let observer = new win.MutationObserver(mutations => {
      // Only consider childList changes or tooltiptext/checked attributes
      // changes. The rest may be mutations caused by the overflowing arrowbox.
      for (let {type, attributeName} of mutations) {
        let isChildList = type === "childList";
        let isAttributes = type === "attributes" &&
                           (attributeName === "checked" ||
                            attributeName === "tooltiptext");
        if (isChildList || isAttributes) {
          hasBreadcrumbsMutated = true;
          break;
        }
      }
    });
    observer.observe(container, {
      attributes: true,
      childList: true,
      subtree: true
    });

    info("Running the test case");
    yield run(inspector);

    info("Wait until the page has mutated");
    yield onContentMutation;

    if (shouldRefresh) {
      info("The breadcrumbs is expected to refresh, so wait for it");
      yield inspector.once("inspector-updated");
    } else {
      ok(!inspector._updateProgress,
        "The breadcrumbs widget is not currently updating");
    }

    is(shouldRefresh, hasBreadcrumbsMutated, "Has the breadcrumbs refreshed?");
    observer.disconnect();

    info("Check the output of the breadcrumbs widget");
    is(container.childNodes.length, output.length, "Correct number of buttons");
    for (let i = 0; i < container.childNodes.length; i++) {
      is(output[i], container.childNodes[i].textContent,
        "Text content for button " + i + " is correct");
    }
  }
});
