/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the inplace-editor behavior.
// This test doesn't open the devtools, it just exercises the inplace-editor
// on test elements in the page

let test = asyncTest(function*() {
  yield addTab("data:text/html,inline editor tests");
  yield testReturnCommit();
  yield testBlurCommit();
  yield testAdvanceCharCommit();
});

function testReturnCommit() {
  info("Testing that pressing return commits the new value");
  let def = promise.defer();

  createInplaceEditorAndClick({
    initial: "explicit initial",
    start: function(editor) {
      is(editor.input.value, "explicit initial", "Explicit initial value should be used.");
      editor.input.value = "Test Value";
      EventUtils.sendKey("return");
    },
    done: onDone("Test Value", true, def)
  });

  return def.promise;
}

function testBlurCommit() {
  info("Testing that bluring the field commits the new value");
  let def = promise.defer();

  createInplaceEditorAndClick({
    start: function(editor) {
      is(editor.input.value, "Edit Me!", "textContent of the span used.");
      editor.input.value = "Test Value";
      editor.input.blur();
    },
    done: onDone("Test Value", true, def)
  });

  return def.promise;
}

function testAdvanceCharCommit() {
  info("Testing that configured advanceChars commit the new value");
  let def = promise.defer();

  createInplaceEditorAndClick({
    advanceChars: ":",
    start: function(editor) {
      let input = editor.input;
      for each (let ch in "Test:") {
        EventUtils.sendChar(ch);
      }
    },
    done: onDone("Test", true, def)
  });

  return def.promise;
}

function testEscapeCancel() {
  info("Testing that escape cancels the new value");
  let def = promise.defer();

  createInplaceEditorAndClick({
    initial: "initial text",
    start: function(editor) {
      editor.input.value = "Test Value";
      EventUtils.sendKey("escape");
    },
    done: onDone("initial text", false, def)
  });

  return def.promise;
}

function onDone(value, isCommit, def) {
  return function(actualValue, actualCommit) {
    info("Inplace-editor's done callback executed, checking its state");
    is(actualValue, value, "The value is correct");
    is(actualCommit, isCommit, "The commit boolean is correct");
    def.resolve();
  }
}

function createInplaceEditorAndClick(options) {
  clearBody();
  let span = options.element = createSpan();

  info("Creating an inplace-editor field");
  editableField(options);

  info("Clicking on the inplace-editor field to turn to edit mode");
  span.click();
}

function clearBody() {
  info("Clearing the page body");
  content.document.body.innerHTML = "";
}

function createSpan() {
  info("Creating a new span element");
  let span = content.document.createElement("span");
  span.setAttribute("tabindex", "0");
  span.textContent = "Edit Me!";
  content.document.body.appendChild(span);
  return span;
}
