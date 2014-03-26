/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test outerHTML edition via the markup-view

loadHelperScript("helper_outerhtml_test_runner.js");

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
  yield runEditOuterHTMLTests(TEST_DATA, inspector);
});
