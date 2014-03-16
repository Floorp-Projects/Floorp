/* Any copyright", " is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that adding various types of attributes to nodes in the markup-view
 * works as expected. Also checks that the changes are properly undoable and
 * redoable. For each step in the test, we:
 * - Create a new DIV
 * - Make the change, check that the change was made as we expect
 * - Undo the change, check that the node is back in its original state
 * - Redo the change, check that the node change was made again correctly.
 */

waitForExplicitFinish();
requestLongerTimeout(2);

let TEST_URL = "data:text/html,<div>markup-view attributes addition test</div>";
let TEST_DATA = [{
  desc: "Add an attribute value without closing \"",
  enteredText: 'style="display: block;',
  expectedAttributes: {
    style: "display: block;"
  }
}, {
  desc: "Add an attribute value without closing '",
  enteredText: "style='display: inline;",
  expectedAttributes: {
    style: "display: inline;"
  }
}, {
  desc: "Add an attribute wrapped with with double quotes double quote in it",
  enteredText: 'style="display: "inline',
  expectedAttributes: {
    style: "display: ",
    inline: ""
  }
}, {
  desc: "Add an attribute wrapped with single quotes with single quote in it",
  enteredText: "style='display: 'inline",
  expectedAttributes: {
    style: "display: ",
    inline: ""
  }
}, {
  desc: "Add an attribute with no value",
  enteredText: "disabled",
  expectedAttributes: {
    disabled: ""
  }
}, {
  desc: "Add multiple attributes with no value",
  enteredText: "disabled autofocus",
  expectedAttributes: {
    disabled: "",
    autofocus: ""
  }
}, {
  desc: "Add multiple attributes with no value, and some with value",
  enteredText: "disabled name='name' data-test='test' autofocus",
  expectedAttributes: {
    disabled: "",
    autofocus: "",
    name: "name",
    'data-test': "test"
  }
}, {
  desc: "Add attribute with xmlns",
  enteredText: "xmlns:edi='http://ecommerce.example.org/schema'",
  expectedAttributes: {
    'xmlns:edi': "http://ecommerce.example.org/schema"
  }
}, {
  desc: "Mixed single and double quotes",
  enteredText: "name=\"hi\" maxlength='not a number'",
  expectedAttributes: {
    maxlength: "not a number",
    name: "hi"
  }
}, {
  desc: "Invalid attribute name",
  enteredText: "x='y' <why-would-you-do-this>=\"???\"",
  expectedAttributes: {
    x: "y"
  }
}, {
  desc: "Double quote wrapped in single quotes",
  enteredText: "x='h\"i'",
  expectedAttributes: {
    x: "h\"i"
  }
}, {
  desc: "Single quote wrapped in double quotes",
  enteredText: "x=\"h'i\"",
  expectedAttributes: {
    x: "h'i"
  }
}, {
  desc: "No quote wrapping",
  enteredText: "a=b x=y data-test=Some spaced data",
  expectedAttributes: {
    a: "b",
    x: "y",
    "data-test": "Some",
    spaced: "",
    data: ""
  }
}, {
  desc: "Duplicate Attributes",
  enteredText: "a=b a='c' a=\"d\"",
  expectedAttributes: {
    a: "b"
  }
}, {
  desc: "Inline styles",
  enteredText: "style=\"font-family: 'Lucida Grande', sans-serif; font-size: 75%;\"",
  expectedAttributes: {
    style: "font-family: 'Lucida Grande', sans-serif; font-size: 75%;"
  }
}, {
  desc: "Object attribute names",
  enteredText: "toString=\"true\" hasOwnProperty=\"false\"",
  expectedAttributes: {
    toString: "true",
    hasOwnProperty: "false"
  }
}, {
  desc: "Add event handlers",
  enteredText: "onclick=\"javascript: throw new Error('wont fire');\" onload=\"alert('here');\"",
  expectedAttributes: {
    onclick: "javascript: throw new Error('wont fire');",
    onload: "alert('here');"
  }
}];

function test() {
  Task.spawn(function() {
    info("Opening the inspector on the test URL");
    let args = yield addTab(TEST_URL).then(openInspector);
    let inspector = args.inspector;
    let markup = inspector.markup;

    info("Selecting the test node");
    let div = getNode("div");
    yield selectNode(div, inspector);
    let editor = getContainerForRawNode(markup, div).editor;

    for (let test of TEST_DATA) {
      info("Starting test: " + test.desc);

      info("Enter the new attribute(s) test: " + test.enteredText);
      let nodeMutated = inspector.once("markupmutation");
      setEditableFieldValue(editor.newAttr, test.enteredText, inspector);
      yield nodeMutated;

      info("Assert that the attribute(s) has/have been applied correctly");
      assertAttributes(div, test.expectedAttributes);

      info("Undo the change");
      yield undoChange(inspector);

      info("Assert that the attribute(s) has/have been removed correctly");
      assertAttributes(div, {});
    }

    yield inspector.once("inspector-updated");

    gBrowser.removeCurrentTab();
  }).then(null, ok.bind(null, false)).then(finish);
}
