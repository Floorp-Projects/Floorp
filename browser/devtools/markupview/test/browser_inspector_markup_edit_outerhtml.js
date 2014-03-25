/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

// Test outerHTML edition via the markup-view

const TEST_DATA = [
  {
    selector: "#one",
    oldHTML: '<div id="one">First <em>Div</em></div>',
    newHTML: '<div id="one">First Div</div>',
    validate: function(pageNode, selectedNode) {
      is(pageNode.textContent, "First Div", "New div has expected text content");
      ok(!getNode("#one em"), "No em remaining")
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
      is(pageNode, selectedNode, "Original element is selected");
      is(pageNode.outerHTML, '<div id="addedAttribute" class="important" disabled="" checked="">addedAttribute</div>',
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
      is(pageNode, selectedNode, "Original element is selected");

      let textNode = pageNode.nextSibling;

      is(textNode.nodeName, "#text", "Sibling is a text element");
      is(textNode.data, " hanging", "New text node has expected text content");
    }
  },
  {
    selector: "#badMarkup2",
    oldHTML: '<div id="badMarkup2">badMarkup2</div>',
    newHTML: '<div id="badMarkup2">badMarkup2</div> hanging<div></div></div></div></body>',
    validate: function(pageNode, selectedNode) {
      is(pageNode, selectedNode, "Original element is selected");

      let textNode = pageNode.nextSibling;

      is(textNode.nodeName, "#text", "Sibling is a text element");
      is(textNode.data, " hanging", "New text node has expected text content");
    }
  },
  {
    selector: "#badMarkup3",
    oldHTML: '<div id="badMarkup3">badMarkup3</div>',
    newHTML: '<div id="badMarkup3">badMarkup3 <em>Emphasized <strong> and strong</div>',
    validate: function(pageNode, selectedNode) {
      is(pageNode, selectedNode, "Original element is selected");

      let em = getNode("#badMarkup3 em");
      let strong = getNode("#badMarkup3 strong");

      is(em.textContent, "Emphasized  and strong", "<em> was auto created");
      is(strong.textContent, " and strong", "<strong> was auto created");
    }
  },
  {
    selector: "#badMarkup4",
    oldHTML: '<div id="badMarkup4">badMarkup4</div>',
    newHTML: '<div id="badMarkup4">badMarkup4</p>',
    validate: function(pageNode, selectedNode) {
      is(pageNode, selectedNode, "Original element is selected");

      let div = getNode("#badMarkup4");
      let p = getNode("#badMarkup4 p");

      is(div.textContent, "badMarkup4", "textContent is correct");
      is(div.tagName, "DIV", "did not change to <p> tag");
      is(p.textContent, "", "The <p> tag has no children");
      is(p.tagName, "P", "Created an empty <p> tag");
    }
  },
  {
    selector: "#badMarkup5",
    oldHTML: '<p id="badMarkup5">badMarkup5</p>',
    newHTML: '<p id="badMarkup5">badMarkup5 <div>with a nested div</div></p>',
    validate: function(pageNode, selectedNode) {
      is(pageNode, selectedNode, "Original element is selected");

      let p = getNode("#badMarkup5");
      let nodiv = getNode("#badMarkup5 div");
      let div = getNode("#badMarkup5 ~ div");

      ok(!nodiv, "The invalid markup got created as a sibling");
      is(p.textContent, "badMarkup5 ", "The <p> tag does not take in the <div> content");
      is(p.tagName, "P", "Did not change to a <div> tag");
      is(div.textContent, "with a nested div", "textContent is correct");
      is(div.tagName, "DIV", "Did not change to <p> tag");
    }
  },
  {
    selector: "#siblings",
    oldHTML: '<div id="siblings">siblings</div>',
    newHTML: '<div id="siblings-before-sibling">before sibling</div>' +
             '<div id="siblings">siblings (updated)</div>' +
             '<div id="siblings-after-sibling">after sibling</div>',
    validate: function(pageNode, selectedNode) {
      let beforeSiblingNode = getNode("#siblings-before-sibling");
      let afterSiblingNode = getNode("#siblings-after-sibling");

      is(beforeSiblingNode, selectedNode, "Sibling has been selected");
      is(pageNode.textContent, "siblings (updated)", "New div has expected text content");
      is(beforeSiblingNode.textContent, "before sibling", "Sibling has been inserted");
      is(afterSiblingNode.textContent, "after sibling", "Sibling has been inserted");
    }
  }
];

const TEST_URL = "data:text/html," +
  "<!DOCTYPE html>" +
  "<head><meta charset='utf-8' /></head>" +
  "<body>" +
  [outer.oldHTML for (outer of TEST_DATA)].join("\n") +
  "</body>" +
  "</html>";

let test = asyncTest(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);

  inspector.markup._frame.focus();

  for (let step of TEST_DATA) {
    yield selectNode(step.selector, inspector);
    yield executeStep(step, inspector);
  }
});

function executeStep(step, inspector) {
  let rawNode = getNode(step.selector);
  let oldNodeFront = inspector.selection.nodeFront;

  // markupmutation fires once the outerHTML is set, with a target
  // as the parent node and a type of "childList".
  return editHTML(step, inspector).then(mutations => {
    // Check to make the sure the correct mutation has fired, and that the
    // parent is selected (this will be reset to the child once the mutation is complete.
    let node = inspector.selection.node;
    let nodeFront = inspector.selection.nodeFront;
    let mutation = mutations[0];
    let isFromOuterHTML = mutation.removed.some(n => n === oldNodeFront);

    ok(isFromOuterHTML, "The node is in the 'removed' list of the mutation");
    is(mutation.type, "childList", "Mutation is a childList after updating outerHTML");
    is(mutation.target, nodeFront, "Parent node is selected immediately after setting outerHTML");

    // Wait for node to be reselected after outerHTML has been set
    return inspector.selection.once("new-node").then(() => {
      // Typically selectedNode will === pageNode, but if a new element has been injected in front
      // of it, this will not be the case.  If this happens.
      let selectedNode = inspector.selection.node;
      let nodeFront = inspector.selection.nodeFront;
      let pageNode = getNode(step.selector);

      if (step.validate) {
        step.validate(pageNode, selectedNode);
      } else {
        is(pageNode, selectedNode, "Original node (grabbed by selector) is selected");
        is(pageNode.outerHTML, step.newHTML, "Outer HTML has been updated");
      }

      // Wait for the inspector to be fully updated to avoid causing errors by
      // abruptly closing hanging requests when the test ends
      return inspector.once("inspector-updated");
    });
  });
}

function editHTML({oldHTML, newHTML}, inspector) {
  // markupmutation fires once the outerHTML is set, with a target
  // as the parent node and a type of "childList".
  let mutated = inspector.once("markupmutation");
  inspector.markup.updateNodeOuterHTML(inspector.selection.nodeFront, newHTML, oldHTML);
  return mutated;
}
