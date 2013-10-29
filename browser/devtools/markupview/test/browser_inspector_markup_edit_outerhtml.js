/* Any copyright", " is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */


function test() {
  let inspector;
  let doc;

  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    doc = content.document;
    waitForFocus(setupTest, content);
  }, true);

  let outerHTMLs = [
    {
      selector: "#one",
      oldHTML: '<div id="one">First <em>Div</em></div>',
      newHTML: '<div id="one">First Div</div>',
      validate: function(pageNode, selectedNode) {
        is (pageNode.textContent, "First Div", "New div has expected text content");
        ok (!doc.querySelector("#one em"), "No em remaining")
      }
    },
    {
      selector: "#removedChildren",
      oldHTML: '<div id="removedChildren">removedChild <i>Italic <b>Bold <u>Underline</u></b></i> Normal</div>',
      newHTML: '<div id="removedChildren">removedChild</div>'
    },
    {
      selector: "#addedChildren",
      oldHTML: '<div id="addedChildren">addedChildren</div>',
      newHTML: '<div id="addedChildren">addedChildren <i>Italic <b>Bold <u>Underline</u></b></i> Normal</div>'
    },
    {
      selector: "#addedAttribute",
      oldHTML: '<div id="addedAttribute">addedAttribute</div>',
      newHTML: '<div id="addedAttribute" class="important" disabled checked>addedAttribute</div>',
      validate: function(pageNode, selectedNode) {
        is (pageNode, selectedNode, "Original element is selected");
        is (pageNode.outerHTML, '<div id="addedAttribute" class="important" disabled="" checked="">addedAttribute</div>',
              "Attributes have been added");
      }
    },
    {
      selector: "#changedTag",
      oldHTML: '<div id="changedTag">changedTag</div>',
      newHTML: '<p id="changedTag" class="important">changedTag</p>'
    },
    {
      selector: "#badMarkup1",
      oldHTML: '<div id="badMarkup1">badMarkup1</div>',
      newHTML: '<div id="badMarkup1">badMarkup1</div> hanging</div>',
      validate: function(pageNode, selectedNode) {
        is (pageNode, selectedNode, "Original element is selected");

        let textNode = pageNode.nextSibling;

        is (textNode.nodeName, "#text", "Sibling is a text element");
        is (textNode.data, " hanging", "New text node has expected text content");
      }
    },
    {
      selector: "#badMarkup2",
      oldHTML: '<div id="badMarkup2">badMarkup2</div>',
      newHTML: '<div id="badMarkup2">badMarkup2</div> hanging<div></div></div></div></body>',
      validate: function(pageNode, selectedNode) {
        is (pageNode, selectedNode, "Original element is selected");

        let textNode = pageNode.nextSibling;

        is (textNode.nodeName, "#text", "Sibling is a text element");
        is (textNode.data, " hanging", "New text node has expected text content");
      }
    },
    {
      selector: "#badMarkup3",
      oldHTML: '<div id="badMarkup3">badMarkup3</div>',
      newHTML: '<div id="badMarkup3">badMarkup3 <em>Emphasized <strong> and strong</div>',
      validate: function(pageNode, selectedNode) {
        is (pageNode, selectedNode, "Original element is selected");

        let em = doc.querySelector("#badMarkup3 em");
        let strong = doc.querySelector("#badMarkup3 strong");

        is (em.textContent, "Emphasized  and strong", "<em> was auto created");
        is (strong.textContent, " and strong", "<strong> was auto created");
      }
    },
    {
      selector: "#badMarkup4",
      oldHTML: '<div id="badMarkup4">badMarkup4</div>',
      newHTML: '<div id="badMarkup4">badMarkup4</p>',
      validate: function(pageNode, selectedNode) {
        is (pageNode, selectedNode, "Original element is selected");

        let div = doc.querySelector("#badMarkup4");
        let p = doc.querySelector("#badMarkup4 p");

        is (div.textContent, "badMarkup4", "textContent is correct");
        is (div.tagName, "DIV", "did not change to <p> tag");
        is (p.textContent, "", "The <p> tag has no children");
        is (p.tagName, "P", "Created an empty <p> tag");
      }
    },
    {
      selector: "#badMarkup5",
      oldHTML: '<p id="badMarkup5">badMarkup5</p>',
      newHTML: '<p id="badMarkup5">badMarkup5 <div>with a nested div</div></p>',
      validate: function(pageNode, selectedNode) {
        is (pageNode, selectedNode, "Original element is selected");

        let p = doc.querySelector("#badMarkup5");
        let nodiv = doc.querySelector("#badMarkup5 div");
        let div = doc.querySelector("#badMarkup5 ~ div");

        ok (!nodiv, "The invalid markup got created as a sibling");
        is (p.textContent, "badMarkup5 ", "The <p> tag does not take in the <div> content");
        is (p.tagName, "P", "Did not change to a <div> tag");
        is (div.textContent, "with a nested div", "textContent is correct");
        is (div.tagName, "DIV", "Did not change to <p> tag");
      }
    },
    {
      selector: "#siblings",
      oldHTML: '<div id="siblings">siblings</div>',
      newHTML: '<div id="siblings-before-sibling">before sibling</div>' +
               '<div id="siblings">siblings (updated)</div>' +
               '<div id="siblings-after-sibling">after sibling</div>',
      validate: function(pageNode, selectedNode) {
        let beforeSiblingNode = doc.querySelector("#siblings-before-sibling");
        let afterSiblingNode = doc.querySelector("#siblings-after-sibling");

        is (beforeSiblingNode, selectedNode, "Sibling has been selected");
        is (pageNode.textContent, "siblings (updated)", "New div has expected text content");
        is (beforeSiblingNode.textContent, "before sibling", "Sibling has been inserted");
        is (afterSiblingNode.textContent, "after sibling", "Sibling has been inserted");
      }
    }
  ];
  content.location = "data:text/html," +
    "<!DOCTYPE html>" +
    "<head><meta charset='utf-8' /></head>" +
    "<body>" +
    [outer.oldHTML for (outer of outerHTMLs) ].join("\n") +
    "</body>" +
    "</html>";

  function setupTest() {
    var target = TargetFactory.forTab(gBrowser.selectedTab);
    gDevTools.showToolbox(target, "inspector").then(function(toolbox) {
      inspector = toolbox.getCurrentPanel();
      inspector.once("inspector-updated", startTests);
    });
  }

  function startTests() {
    inspector.markup._frame.focus();
    nextStep(0);
  }

  function nextStep(cursor) {
    if (cursor >= outerHTMLs.length) {
      testBody();
      return;
    }

    let currentTestData = outerHTMLs[cursor];
    let selector = currentTestData.selector;
    let oldHTML = currentTestData.oldHTML;
    let newHTML = currentTestData.newHTML;
    let rawNode = doc.querySelector(selector);

    inspector.selection.once("new-node", () => {

      let oldNodeFront = inspector.selection.nodeFront;

      // markupmutation fires once the outerHTML is set, with a target
      // as the parent node and a type of "childList".
      inspector.once("markupmutation", (e, aMutations) => {

        // Check to make the sure the correct mutation has fired, and that the
        // parent is selected (this will be reset to the child once the mutation is complete.
        let node = inspector.selection.node;
        let nodeFront = inspector.selection.nodeFront;
        let mutation = aMutations[0];
        let isFromOuterHTML = mutation.removed.some((n) => {
          return n === oldNodeFront;
        });

        ok (isFromOuterHTML, "The node is in the 'removed' list of the mutation");
        is (mutation.type, "childList", "Mutation is a childList after updating outerHTML");
        is (mutation.target, nodeFront, "Parent node is selected immediately after setting outerHTML");

        // Wait for node to be reselected after outerHTML has been set
        inspector.selection.once("new-node", () => {

          // Typically selectedNode will === pageNode, but if a new element has been injected in front
          // of it, this will not be the case.  If this happens.
          let selectedNode = inspector.selection.node;
          let nodeFront = inspector.selection.nodeFront;
          let pageNode = doc.querySelector(selector);

          if (currentTestData.validate) {
            currentTestData.validate(pageNode, selectedNode);
          } else {
            is (pageNode, selectedNode, "Original node (grabbed by selector) is selected");
            is (pageNode.outerHTML, newHTML, "Outer HTML has been updated");
          }

          nextStep(cursor + 1);
        });

      });

      is (inspector.selection.node, rawNode, "Selection is on the correct node");
      inspector.markup.updateNodeOuterHTML(inspector.selection.nodeFront, newHTML, oldHTML);
    });

    inspector.selection.setNode(rawNode);
  }

  function testBody() {
    let body = doc.querySelector("body");
    let bodyHTML = '<body id="updated"><p></p></body>';
    let bodyFront = inspector.markup.walker.frontForRawNode(body);
    inspector.once("markupmutation", (e, aMutations) => {
      is (doc.querySelector("body").outerHTML, bodyHTML, "<body> HTML has been updated");
      is (doc.querySelectorAll("head").length, 1, "no extra <head>s have been added");
      testHead();
    });
    inspector.markup.updateNodeOuterHTML(bodyFront, bodyHTML, body.outerHTML);
  }

  function testHead() {
    let head = doc.querySelector("head");
    let headHTML = '<head id="updated"><title>New Title</title><script>window.foo="bar";</script></head>';
    let headFront = inspector.markup.walker.frontForRawNode(head);
    inspector.once("markupmutation", (e, aMutations) => {
      is (doc.title, "New Title", "New title has been added");
      is (doc.defaultView.foo, undefined, "Script has not been executed");
      is (doc.querySelector("head").outerHTML, headHTML, "<head> HTML has been updated");
      is (doc.querySelectorAll("body").length, 1, "no extra <body>s have been added");
      testDocumentElement();
    });
    inspector.markup.updateNodeOuterHTML(headFront, headHTML, head.outerHTML);
  }

  function testDocumentElement() {
    let docElement = doc.documentElement;
    let docElementHTML = '<html id="updated" foo="bar"><head><title>Updated from document element</title><script>window.foo="bar";</script></head><body><p>Hello</p></body></html>';
    let docElementFront = inspector.markup.walker.frontForRawNode(docElement);
    inspector.once("markupmutation", (e, aMutations) => {
      is (doc.title, "Updated from document element", "New title has been added");
      is (doc.defaultView.foo, undefined, "Script has not been executed");
      is (doc.documentElement.id, "updated", "<html> ID has been updated");
      is (doc.documentElement.className, "", "<html> class has been updated");
      is (doc.documentElement.getAttribute("foo"), "bar", "<html> attribute has been updated");
      is (doc.documentElement.outerHTML, docElementHTML, "<html> HTML has been updated");
      is (doc.querySelectorAll("head").length, 1, "no extra <head>s have been added");
      is (doc.querySelectorAll("body").length, 1, "no extra <body>s have been added");
      is (doc.body.textContent, "Hello", "document.body.textContent has been updated");
      testDocumentElement2();
    });
    inspector.markup.updateNodeOuterHTML(docElementFront, docElementHTML, docElement.outerHTML);
  }

  function testDocumentElement2() {
    let docElement = doc.documentElement;
    let docElementHTML = '<html class="updated" id="somethingelse"><head><title>Updated again from document element</title><script>window.foo="bar";</script></head><body><p>Hello again</p></body></html>';
    let docElementFront = inspector.markup.walker.frontForRawNode(docElement);
    inspector.once("markupmutation", (e, aMutations) => {
      is (doc.title, "Updated again from document element", "New title has been added");
      is (doc.defaultView.foo, undefined, "Script has not been executed");
      is (doc.documentElement.id, "somethingelse", "<html> ID has been updated");
      is (doc.documentElement.className, "updated", "<html> class has been updated");
      is (doc.documentElement.getAttribute("foo"), null, "<html> attribute has been removed");
      is (doc.documentElement.outerHTML, docElementHTML, "<html> HTML has been updated");
      is (doc.querySelectorAll("head").length, 1, "no extra <head>s have been added");
      is (doc.querySelectorAll("body").length, 1, "no extra <body>s have been added");
      is (doc.body.textContent, "Hello again", "document.body.textContent has been updated");
      finishUp();
    });
    inspector.markup.updateNodeOuterHTML(docElementFront, docElementHTML, docElement.outerHTML);
  }

  function finishUp() {
    doc = inspector = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
