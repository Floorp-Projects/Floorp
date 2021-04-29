/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_outerhtml_test_runner.js */
"use strict";

// Test outerHTML edition via the markup-view

loadHelperScript("helper_outerhtml_test_runner.js");
requestLongerTimeout(2);

const TEST_DATA = [
  {
    selector: "#badMarkup1",
    oldHTML: '<div id="badMarkup1">badMarkup1</div>',
    newHTML: '<div id="badMarkup1">badMarkup1</div> hanging</div>',
    validate: async function({ pageNodeFront, selectedNodeFront, testActor }) {
      is(pageNodeFront, selectedNodeFront, "Original element is selected");

      const [textNodeName, textNodeData] = await SpecialPowers.spawn(
        gBrowser.selectedBrowser,
        [],
        () => {
          const node = content.document.querySelector("#badMarkup1")
            .nextSibling;
          return [node.nodeName, node.data];
        }
      );
      is(textNodeName, "#text", "Sibling is a text element");
      is(textNodeData, " hanging", "New text node has expected text content");
    },
  },
  {
    selector: "#badMarkup2",
    oldHTML: '<div id="badMarkup2">badMarkup2</div>',
    newHTML:
      '<div id="badMarkup2">badMarkup2</div> hanging<div></div>' +
      "</div></div></body>",
    validate: async function({ pageNodeFront, selectedNodeFront, testActor }) {
      is(pageNodeFront, selectedNodeFront, "Original element is selected");

      const [textNodeName, textNodeData] = await SpecialPowers.spawn(
        gBrowser.selectedBrowser,
        [],
        () => {
          const node = content.document.querySelector("#badMarkup2")
            .nextSibling;
          return [node.nodeName, node.data];
        }
      );
      is(textNodeName, "#text", "Sibling is a text element");
      is(textNodeData, " hanging", "New text node has expected text content");
    },
  },
  {
    selector: "#badMarkup3",
    oldHTML: '<div id="badMarkup3">badMarkup3</div>',
    newHTML:
      '<div id="badMarkup3">badMarkup3 <em>Emphasized <strong> ' +
      "and strong</div>",
    validate: async function({ pageNodeFront, selectedNodeFront, testActor }) {
      is(pageNodeFront, selectedNodeFront, "Original element is selected");

      const emText = await testActor.getProperty(
        "#badMarkup3 em",
        "textContent"
      );
      const strongText = await testActor.getProperty(
        "#badMarkup3 strong",
        "textContent"
      );
      is(emText, "Emphasized  and strong", "<em> was auto created");
      is(strongText, " and strong", "<strong> was auto created");
    },
  },
  {
    selector: "#badMarkup4",
    oldHTML: '<div id="badMarkup4">badMarkup4</div>',
    newHTML: '<div id="badMarkup4">badMarkup4</p>',
    validate: async function({ pageNodeFront, selectedNodeFront, testActor }) {
      is(pageNodeFront, selectedNodeFront, "Original element is selected");

      const divText = await testActor.getProperty("#badMarkup4", "textContent");
      const divTag = await testActor.getProperty("#badMarkup4", "tagName");

      const pText = await testActor.getProperty("#badMarkup4 p", "textContent");
      const pTag = await testActor.getProperty("#badMarkup4 p", "tagName");

      is(divText, "badMarkup4", "textContent is correct");
      is(divTag, "DIV", "did not change to <p> tag");
      is(pText, "", "The <p> tag has no children");
      is(pTag, "P", "Created an empty <p> tag");
    },
  },
  {
    selector: "#badMarkup5",
    oldHTML: '<p id="badMarkup5">badMarkup5</p>',
    newHTML: '<p id="badMarkup5">badMarkup5 <div>with a nested div</div></p>',
    validate: async function({ pageNodeFront, selectedNodeFront, testActor }) {
      is(pageNodeFront, selectedNodeFront, "Original element is selected");

      const num = await testActor.getNumberOfElementMatches("#badMarkup5 div");

      const pText = await testActor.getProperty("#badMarkup5", "textContent");
      const pTag = await testActor.getProperty("#badMarkup5", "tagName");

      const divText = await testActor.getProperty(
        "#badMarkup5 ~ div",
        "textContent"
      );
      const divTag = await testActor.getProperty(
        "#badMarkup5 ~ div",
        "tagName"
      );

      is(num, 0, "The invalid markup got created as a sibling");
      is(pText, "badMarkup5 ", "The p tag does not take in the div content");
      is(pTag, "P", "Did not change to a <div> tag");
      is(divText, "with a nested div", "textContent is correct");
      is(divTag, "DIV", "Did not change to <p> tag");
    },
  },
];

const TEST_URL =
  "data:text/html," +
  "<!DOCTYPE html>" +
  "<head><meta charset='utf-8' /></head>" +
  "<body>" +
  TEST_DATA.map(outer => outer.oldHTML).join("\n") +
  "</body>" +
  "</html>";

add_task(async function() {
  const { inspector, testActor } = await openInspectorForURL(TEST_URL);
  inspector.markup._frame.focus();
  await runEditOuterHTMLTests(TEST_DATA, inspector, testActor);
});
