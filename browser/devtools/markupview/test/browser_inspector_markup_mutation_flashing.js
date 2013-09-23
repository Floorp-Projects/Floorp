/* Any copyright", " is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  waitForExplicitFinish();

  // Will hold the doc we're viewing
  let contentTab;
  let doc;
  let listElement;

  // Holds the MarkupTool object we're testing.
  let markup;
  let inspector;

  // Then create the actual dom we're inspecting...
  contentTab = gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupTest, content);
  }, true);
  content.location = "http://mochi.test:8888/browser/browser/devtools/markupview/test/browser_inspector_markup_mutation_flashing.html";

  function setupTest() {
    var target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
      inspector = toolbox.getCurrentPanel();
      startTests();
    });
  }

  function startTests() {
    markup = inspector.markup;

    // Get the content UL element
    listElement = doc.querySelector(".list");

    // Making sure children are expanded
    inspector.selection.setNode(listElement.lastElementChild);
    inspector.once("inspector-updated", () => {
      // testData contains a list of mutations to test
      // Each array item is an object with:
      // - mutate: a function that should make changes to the content DOM
      // - assert: a function that should test flashing background
      let testData = [{
        // Adding a new node should flash the new node
        mutate: () => {
          let newLi = doc.createElement("LI");
          newLi.textContent = "new list item";
          listElement.appendChild(newLi);
        },
        assert: () => {
          assertNodeFlashing(listElement.lastElementChild);
        }
      }, {
        // Removing a node should flash its parent
        mutate: () => {
          listElement.removeChild(listElement.lastElementChild);
        },
        assert: () => {
          assertNodeFlashing(listElement);
        }
      }, {
        // Re-appending an existing node should only flash this node
        mutate: () => {
          listElement.appendChild(listElement.firstElementChild);
        },
        assert: () => {
          assertNodeFlashing(listElement.lastElementChild);
        }
      }, {
        // Adding an attribute should flash the node
        mutate: () => {
          listElement.setAttribute("name-" + Date.now(), "value-" + Date.now());
        },
        assert: () => {
          assertNodeFlashing(listElement);
        }
      }, {
        // Editing an attribute should flash the node
        mutate: () => {
          listElement.setAttribute("class", "list value-" + Date.now());
        },
        assert: () => {
          assertNodeFlashing(listElement);
        }
      }, {
        // Removing an attribute should flash the node
        mutate: () => {
          listElement.removeAttribute("class");
        },
        assert: () => {
          assertNodeFlashing(listElement);
        }
      }];
      testMutation(testData, 0);
    });
  }

  function testMutation(testData, cursor) {
    if (cursor < testData.length) {
      let {mutate, assert} = testData[cursor];
      mutate();
      inspector.once("markupmutation", () => {
        assert();
        testMutation(testData, cursor + 1);
      });
    } else {
      endTests();
    }
  }

  function endTests() {
    gBrowser.removeTab(contentTab);
    doc = inspector = contentTab = markup = listElement = null;
    finish();
  }

  function assertNodeFlashing(rawNode) {
    let container = getContainerForRawNode(markup, rawNode);

    if(!container) {
      ok(false, "Node not found");
    } else {
      ok(container.highlighter.classList.contains("theme-bg-contrast"),
        "Node is flashing");
    }
  }
}
