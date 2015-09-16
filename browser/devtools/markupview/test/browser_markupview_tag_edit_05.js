/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that adding various types of attributes to nodes in the markup-view
// works as expected. Also checks that the changes are properly undoable and
// redoable. For each step in the test, we:
// - Create a new DIV
// - Make the change, check that the change was made as we expect
// - Undo the change, check that the node is back in its original state
// - Redo the change, check that the node change was made again correctly.

loadHelperScript("helper_attributes_test_runner.js");

var TEST_URL = "data:text/html,<div>markup-view attributes addition test</div>";
var TEST_DATA = [{
  desc: "Add an attribute value without closing \"",
  text: 'style="display: block;',
  expectedAttributes: {
    style: "display: block;"
  }
}, {
  desc: "Add an attribute value without closing '",
  text: "style='display: inline;",
  expectedAttributes: {
    style: "display: inline;"
  }
}, {
  desc: "Add an attribute wrapped with with double quotes double quote in it",
  text: 'style="display: "inline',
  expectedAttributes: {
    style: "display: ",
    inline: ""
  }
}, {
  desc: "Add an attribute wrapped with single quotes with single quote in it",
  text: "style='display: 'inline",
  expectedAttributes: {
    style: "display: ",
    inline: ""
  }
}, {
  desc: "Add an attribute with no value",
  text: "disabled",
  expectedAttributes: {
    disabled: ""
  }
}, {
  desc: "Add multiple attributes with no value",
  text: "disabled autofocus",
  expectedAttributes: {
    disabled: "",
    autofocus: ""
  }
}, {
  desc: "Add multiple attributes with no value, and some with value",
  text: "disabled name='name' data-test='test' autofocus",
  expectedAttributes: {
    disabled: "",
    autofocus: "",
    name: "name",
    'data-test': "test"
  }
}, {
  desc: "Add attribute with xmlns",
  text: "xmlns:edi='http://ecommerce.example.org/schema'",
  expectedAttributes: {
    'xmlns:edi': "http://ecommerce.example.org/schema"
  }
}];

add_task(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);
  yield runAddAttributesTests(TEST_DATA, "div", inspector)
});
