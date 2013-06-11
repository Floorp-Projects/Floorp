/* Any copyright", " is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that various mutations to the dom update the markup tool correctly.
 * The test for comparing the markup tool to the real dom is a bit weird:
 * - Select the text in the markup tool
 * - Parse that as innerHTML in a document we've created for the purpose.
 * - Remove extraneous whitespace in that tree
 * - Compare it to the real dom with isEqualNode.
 */

function fail(err) {
  ok(false, err)
}

function test() {
  waitForExplicitFinish();

  // Will hold the doc we're viewing
  let contentTab;
  let doc;

  // Holds the MarkupTool object we're testing.
  let markup;

  // Holds the document we use to help re-parse the markup tool's output.
  let parseTab;
  let parseDoc;

  let inspector;

  // Strip whitespace from a node and its children.
  function stripWhitespace(node)
  {
    node.normalize();
    let iter = node.ownerDocument.createNodeIterator(node, NodeFilter.SHOW_TEXT + NodeFilter.SHOW_COMMENT,
      null);

    while ((node = iter.nextNode())) {
      node.nodeValue = node.nodeValue.replace(/\s+/g, '');
      if (node.nodeType == Node.TEXT_NODE &&
        !/[^\s]/.exec(node.nodeValue)) {
        node.parentNode.removeChild(node);
      }
    }
  }

  // Verify that the markup in the tool is the same as the markup in the document.
  function checkMarkup()
  {
    return markup.expandAll().then(checkMarkup2);
  }

  function checkMarkup2()
  {
    let contentNode = doc.querySelector("body");
    let panelNode = getContainerForRawNode(markup, contentNode).elt;
    let parseNode = parseDoc.querySelector("body");

    // Grab the text from the markup panel...
    let sel = panelNode.ownerDocument.defaultView.getSelection();
    sel.selectAllChildren(panelNode);

    // Parse it
    parseNode.outerHTML = sel;
    parseNode = parseDoc.querySelector("body");

    // Pull whitespace out of text and comment nodes, there will
    // be minor unimportant differences.
    stripWhitespace(parseNode);

    ok(contentNode.isEqualNode(parseNode), "Markup panel should match document.");
  }

  // All the mutation types we want to test.
  let mutations = [
    // Add an attribute
    function() {
      let node1 = doc.querySelector("#node1");
      node1.setAttribute("newattr", "newattrval");
    },
    function() {
      let node1 = doc.querySelector("#node1");
      node1.removeAttribute("newattr");
    },
    function() {
      let node1 = doc.querySelector("#node1");
      node1.textContent = "newtext";
    },
    function() {
      let node2 = doc.querySelector("#node2");
      node2.innerHTML = "<div><span>foo</span></div>";
    },

    function() {
      let node4 = doc.querySelector("#node4");
      while (node4.firstChild) {
        node4.removeChild(node4.firstChild);
      }
    },
    function() {
      // Move a child to a new parent.
      let node17 = doc.querySelector("#node17");
      let node1 = doc.querySelector("#node2");
      node1.appendChild(node17);
    },

    function() {
      // Swap a parent and child element, putting them in the same tree.
      // body
      //  node1
      //  node18
      //    node19
      //      node20
      //        node21
      // will become:
      // body
      //   node1
      //     node20
      //      node21
      //      node18
      //        node19
      let node18 = doc.querySelector("#node18");
      let node20 = doc.querySelector("#node20");

      let node1 = doc.querySelector("#node1");

      node1.appendChild(node20);
      node20.appendChild(node18);
    },
  ];

  // Create the helper tab for parsing...
  parseTab = gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    parseDoc = content.document;

    // Then create the actual dom we're inspecting...
    contentTab = gBrowser.selectedTab = gBrowser.addTab();
    gBrowser.selectedBrowser.addEventListener("load", function onload2() {
      gBrowser.selectedBrowser.removeEventListener("load", onload2, true);
      doc = content.document;
      // Strip whitespace from the doc for easier comparison.
      stripWhitespace(doc.documentElement);
      waitForFocus(setupTest, content);
    }, true);
    content.location = "http://mochi.test:8888/browser/browser/devtools/markupview/test/browser_inspector_markup_mutation.html";
  }, true);

  content.location = "data:text/html,<html></html>";

  function setupTest() {
    var target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
      inspector = toolbox.getCurrentPanel();
      startTests();
    });
  }

  function startTests() {
    markup = inspector.markup;
    checkMarkup().then(() => {
      nextStep(0);
    }).then(null, fail);
  }

  function nextStep(cursor) {
    if (cursor >= mutations.length) {
      finishUp();
      return;
    }
    mutations[cursor]();
    inspector.once("markupmutation", function() {
      executeSoon(function() {
        checkMarkup().then(() => {
          nextStep(cursor + 1);
        }).then(null, fail);
      });
    });
  }

  function finishUp() {
    doc = inspector = null;
    gBrowser.removeTab(contentTab);
    gBrowser.removeTab(parseTab);
    finish();
  }
}
