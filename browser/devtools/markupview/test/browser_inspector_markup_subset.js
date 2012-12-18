/* Any copyright", " is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that the markup view loads only as many nodes as specified
 * by the devtools.markup.pagesize preference.
 */

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("devtools.markup.pagesize");
});
Services.prefs.setIntPref("devtools.markup.pagesize", 5);


function test() {
  waitForExplicitFinish();

  // Will hold the doc we're viewing
  let doc;

  let inspector;

  // Holds the MarkupTool object we're testing.
  let markup;

  function assertChildren(expected)
  {
    let container = markup.getContainer(doc.querySelector("body"));
    let found = [];
    for (let child of container.children.children) {
      if (child.classList.contains("more-nodes")) {
        found += "*more*";
      } else {
        found += child.container.node.getAttribute("id");
      }
    }
    is(expected, found, "Got the expected children.");
  }

  function forceReload()
  {
    let container = markup.getContainer(doc.querySelector("body"));
    container.childrenDirty = true;
  }

  let selections = [
    {
      desc: "Select the first item",
      selector: "#a",
      before: function() {
      },
      after: function() {
        assertChildren("abcde*more*");
      }
    },
    {
      desc: "Select the last item",
      selector: "#z",
      before: function() {},
      after: function() {
        assertChildren("*more*vwxyz");
      }
    },
    {
      desc: "Select an already-visible item",
      selector: "#v",
      before: function() {},
      after: function() {
        // Because "v" was already visible, we shouldn't have loaded
        // a different page.
        assertChildren("*more*vwxyz");
      },
    },
    {
      desc: "Verify childrenDirty reloads the page",
      selector: "#w",
      before: function() {
        forceReload();
      },
      after: function() {
        // But now that we don't already have a loaded page, selecting
        // w should center around w.
        assertChildren("*more*uvwxy*more*");
      },
    },
  ];

  // Create the helper tab for parsing...
  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupTest, content);
  }, true);
  content.location = "http://mochi.test:8888/browser/browser/devtools/markupview/test/browser_inspector_markup_subset.html";

  function setupTest() {
    var target = TargetFactory.forTab(gBrowser.selectedTab);
    let toolbox = gDevTools.openToolboxForTab(target, "inspector");
    toolbox.once("inspector-selected", function SE_selected(id, aInspector) {
      inspector = aInspector;
      markup = inspector.markup;
      runNextSelection();
    });
  }

  function runTests() {
    inspector.selection.once("new-node", startTests);
    executeSoon(function() {
      inspector.selection.setNode(doc.body);
    });
  }

  function runNextSelection() {
    let selection = selections.shift();
    if (!selection) {
      clickMore();
      return;
    }

    info(selection.desc);
    selection.before();
    inspector.selection.once("new-node", function() {
      selection.after();
      runNextSelection();
    });
    inspector.selection.setNode(doc.querySelector(selection.selector));
  }

  function clickMore() {
    info("Check that clicking more loads the whole thing.");
    // Make sure that clicking the "more" button loads all the nodes.
    let container = markup.getContainer(doc.querySelector("body"));
    let button = container.elt.querySelector("button");
    button.click();
    assertChildren("abcdefghijklmnopqrstuvwxyz");
    finishUp();
  }

  function finishUp() {
    doc = inspector = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
