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

let TEST_URL = "data:text/html,<div>markup-view attributes addition test</div>";
let TEST_DATA = [{
  desc: "Mixed single and double quotes",
  text: "name=\"hi\" maxlength='not a number'",
  expectedAttributes: {
    maxlength: "not a number",
    name: "hi"
  }
}, {
  desc: "Invalid attribute name",
  text: "x='y' <why-would-you-do-this>=\"???\"",
  expectedAttributes: {
    x: "y"
  }
}, {
  desc: "Double quote wrapped in single quotes",
  text: "x='h\"i'",
  expectedAttributes: {
    x: "h\"i"
  }
}, {
  desc: "Single quote wrapped in double quotes",
  text: "x=\"h'i\"",
  expectedAttributes: {
    x: "h'i"
  }
}, {
  desc: "No quote wrapping",
  text: "a=b x=y data-test=Some spaced data",
  expectedAttributes: {
    a: "b",
    x: "y",
    "data-test": "Some",
    spaced: "",
    data: ""
  }
}, {
  desc: "Duplicate Attributes",
  text: "a=b a='c' a=\"d\"",
  expectedAttributes: {
    a: "b"
  }
}, {
  desc: "Inline styles",
  text: "style=\"font-family: 'Lucida Grande', sans-serif; font-size: 75%;\"",
  expectedAttributes: {
    style: "font-family: 'Lucida Grande', sans-serif; font-size: 75%;"
  }
}, {
  desc: "Object attribute names",
  text: "toString=\"true\" hasOwnProperty=\"false\"",
  expectedAttributes: {
    toString: "true",
    hasOwnProperty: "false"
  }
}, {
  desc: "Add event handlers",
  text: "onclick=\"javascript: throw new Error('wont fire');\" onload=\"alert('here');\"",
  expectedAttributes: {
    onclick: "javascript: throw new Error('wont fire');",
    onload: "alert('here');"
  }
}];

let test = asyncTest(function*() {
  let {inspector} = yield addTab(TEST_URL).then(openInspector);
  yield runAddAttributesTests(TEST_DATA, "div", inspector)
});
