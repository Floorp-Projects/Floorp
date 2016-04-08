/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_outerhtml_test_runner.js */
"use strict";

// Test outerHTML edition via the markup-view

loadHelperScript("helper_outerhtml_test_runner.js");

const TEST_DATA = [{
  selector: "#one",
  oldHTML: '<div id="one">First <em>Div</em></div>',
  newHTML: '<div id="one">First Div</div>',
  validate: function* ({pageNodeFront, selectedNodeFront, testActor}) {
    let text = yield testActor.getProperty("#one", "textContent");
    is(text, "First Div", "New div has expected text content");
    let num = yield testActor.getNumberOfElementMatches("#one em");
    is(num, 0, "No em remaining");
  }
}, {
  selector: "#removedChildren",
  oldHTML: "<div id=\"removedChildren\">removedChild " +
           "<i>Italic <b>Bold <u>Underline</u></b></i> Normal</div>",
  newHTML: "<div id=\"removedChildren\">removedChild</div>"
}, {
  selector: "#addedChildren",
  oldHTML: '<div id="addedChildren">addedChildren</div>',
  newHTML: "<div id=\"addedChildren\">addedChildren " +
           "<i>Italic <b>Bold <u>Underline</u></b></i> Normal</div>"
}, {
  selector: "#addedAttribute",
  oldHTML: '<div id="addedAttribute">addedAttribute</div>',
  newHTML: "<div id=\"addedAttribute\" class=\"important\" disabled checked>" +
           "addedAttribute</div>",
  validate: function* ({pageNodeFront, selectedNodeFront, testActor}) {
    is(pageNodeFront, selectedNodeFront, "Original element is selected");
    let html = yield testActor.getProperty("#addedAttribute", "outerHTML");
    is(html, "<div id=\"addedAttribute\" class=\"important\" disabled=\"\" " +
       "checked=\"\">addedAttribute</div>", "Attributes have been added");
  }
}, {
  selector: "#changedTag",
  oldHTML: '<div id="changedTag">changedTag</div>',
  newHTML: '<p id="changedTag" class="important">changedTag</p>'
}, {
  selector: "#siblings",
  oldHTML: '<div id="siblings">siblings</div>',
  newHTML: '<div id="siblings-before-sibling">before sibling</div>' +
           '<div id="siblings">siblings (updated)</div>' +
           '<div id="siblings-after-sibling">after sibling</div>',
  validate: function* ({selectedNodeFront, inspector, testActor}) {
    let beforeSiblingFront = yield getNodeFront("#siblings-before-sibling",
                                                inspector);
    is(beforeSiblingFront, selectedNodeFront, "Sibling has been selected");

    let text = yield testActor.getProperty("#siblings", "textContent");
    is(text, "siblings (updated)", "New div has expected text content");

    let beforeText = yield testActor.getProperty("#siblings-before-sibling",
                                                 "textContent");
    is(beforeText, "before sibling", "Sibling has been inserted");

    let afterText = yield testActor.getProperty("#siblings-after-sibling",
                                                "textContent");
    is(afterText, "after sibling", "Sibling has been inserted");
  }
}];

const TEST_URL = "data:text/html," +
  "<!DOCTYPE html>" +
  "<head><meta charset='utf-8' /></head>" +
  "<body>" +
  TEST_DATA.map(outer => outer.oldHTML).join("\n") +
  "</body>" +
  "</html>";

add_task(function* () {
  let {inspector, testActor} = yield openInspectorForURL(TEST_URL);
  inspector.markup._frame.focus();
  yield runEditOuterHTMLTests(TEST_DATA, inspector, testActor);
});
