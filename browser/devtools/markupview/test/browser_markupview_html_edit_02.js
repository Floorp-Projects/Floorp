/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test outerHTML edition via the markup-view

loadHelperScript("helper_outerhtml_test_runner.js");

const TEST_DATA = [
  {
    selector: "#badMarkup1",
    oldHTML: '<div id="badMarkup1">badMarkup1</div>',
    newHTML: '<div id="badMarkup1">badMarkup1</div> hanging</div>',
    validate: function*(pageNode, pageNodeFront, selectedNodeFront, inspector) {
      is(pageNodeFront, selectedNodeFront, "Original element is selected");

      let textNode = pageNode.nextSibling;

      is(textNode.nodeName, "#text", "Sibling is a text element");
      is(textNode.data, " hanging", "New text node has expected text content");
    }
  },
  {
    selector: "#badMarkup2",
    oldHTML: '<div id="badMarkup2">badMarkup2</div>',
    newHTML: '<div id="badMarkup2">badMarkup2</div> hanging<div></div></div></div></body>',
    validate: function*(pageNode, pageNodeFront, selectedNodeFront, inspector) {
      is(pageNodeFront, selectedNodeFront, "Original element is selected");

      let textNode = pageNode.nextSibling;

      is(textNode.nodeName, "#text", "Sibling is a text element");
      is(textNode.data, " hanging", "New text node has expected text content");
    }
  },
  {
    selector: "#badMarkup3",
    oldHTML: '<div id="badMarkup3">badMarkup3</div>',
    newHTML: '<div id="badMarkup3">badMarkup3 <em>Emphasized <strong> and strong</div>',
    validate: function*(pageNode, pageNodeFront, selectedNodeFront, inspector) {
      is(pageNodeFront, selectedNodeFront, "Original element is selected");

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
    validate: function*(pageNode, pageNodeFront, selectedNodeFront, inspector) {
      is(pageNodeFront, selectedNodeFront, "Original element is selected");

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
    validate: function*(pageNode, pageNodeFront, selectedNodeFront, inspector) {
      is(pageNodeFront, selectedNodeFront, "Original element is selected");

      let p = getNode("#badMarkup5");
      let nodiv = getNode("#badMarkup5 div");
      let div = getNode("#badMarkup5 ~ div");

      ok(!nodiv, "The invalid markup got created as a sibling");
      is(p.textContent, "badMarkup5 ", "The <p> tag does not take in the <div> content");
      is(p.tagName, "P", "Did not change to a <div> tag");
      is(div.textContent, "with a nested div", "textContent is correct");
      is(div.tagName, "DIV", "Did not change to <p> tag");
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
  yield runEditOuterHTMLTests(TEST_DATA, inspector);
});
