/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that a node can be deleted from the markup-view with the delete key.
// Also checks that after deletion the correct element is highlighted.
// The next sibling is preferred, but the parent is a fallback.

const HTML = `<style type="text/css">
                #pseudo::before { content: 'before'; }
                #pseudo::after { content: 'after'; }
              </style>
              <div id="parent">
                <div id="first"></div>
                <div id="second"></div>
                <div id="third"></div>
              </div>
              <div id="only-child">
                <div id="fourth"></div>
              </div>
              <div id="pseudo">
                <div id="fifth"></div>
              </div>`;
const TEST_URL = "data:text/html;charset=utf-8," + encodeURIComponent(HTML);

// List of all the test cases. Each item is an object with the following props:
// - selector: the css selector of the node that should be selected
// - focusedSelector: the css selector of the node we expect to be selected as
//   a result of the deletion
// - pseudo: (optional) if the focused node is actually supposed to be a pseudo element
//   of the specified selector.
// Note that after each test case, undo is called.
const TEST_DATA = [
  {
    selector: "#first",
    focusedSelector: "#second",
  },
  {
    selector: "#second",
    focusedSelector: "#third",
  },
  {
    selector: "#third",
    focusedSelector: "#second",
  },
  {
    selector: "#fourth",
    focusedSelector: "#only-child",
  },
  {
    selector: "#fifth",
    focusedSelector: "#pseudo",
    pseudo: "after",
  },
];

add_task(async function() {
  const { inspector } = await openInspectorForURL(TEST_URL);

  for (const data of TEST_DATA) {
    await checkDeleteAndSelection(inspector, "delete", data);
  }
});
