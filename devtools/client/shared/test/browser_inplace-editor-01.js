/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_inplace_editor.js */

"use strict";

loadHelperScript("helper_inplace_editor.js");

// Test the inplace-editor behavior.

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8,inline editor tests");
  let [host, , doc] = yield createHost();

  yield testMultipleInitialization(doc);
  yield testReturnCommit(doc);
  yield testBlurCommit(doc);
  yield testAdvanceCharCommit(doc);
  yield testAdvanceCharsFunction(doc);
  yield testEscapeCancel(doc);

  host.destroy();
  gBrowser.removeCurrentTab();
});

function testMultipleInitialization(doc) {
  doc.body.innerHTML = "";
  let options = {};
  let span = options.element = createSpan(doc);

  info("Creating multiple inplace-editor fields");
  editableField(options);
  editableField(options);

  info("Clicking on the inplace-editor field to turn to edit mode");
  span.click();

  is(span.style.display, "none", "The original <span> is hidden");
  is(doc.querySelectorAll("input").length, 1, "Only one <input>");
  is(doc.querySelectorAll("span").length, 2,
    "Correct number of <span> elements");
  is(doc.querySelectorAll("span.autosizer").length, 1,
    "There is an autosizer element");
}

function testReturnCommit(doc) {
  info("Testing that pressing return commits the new value");
  let def = promise.defer();

  createInplaceEditorAndClick({
    initial: "explicit initial",
    start: function (editor) {
      is(editor.input.value, "explicit initial",
        "Explicit initial value should be used.");
      editor.input.value = "Test Value";
      EventUtils.sendKey("return");
    },
    done: onDone("Test Value", true, def)
  }, doc);

  return def.promise;
}

function testBlurCommit(doc) {
  info("Testing that bluring the field commits the new value");
  let def = promise.defer();

  createInplaceEditorAndClick({
    start: function (editor) {
      is(editor.input.value, "Edit Me!", "textContent of the span used.");
      editor.input.value = "Test Value";
      editor.input.blur();
    },
    done: onDone("Test Value", true, def)
  }, doc, "Edit Me!");

  return def.promise;
}

function testAdvanceCharCommit(doc) {
  info("Testing that configured advanceChars commit the new value");
  let def = promise.defer();

  createInplaceEditorAndClick({
    advanceChars: ":",
    start: function (editor) {
      EventUtils.sendString("Test:");
    },
    done: onDone("Test", true, def)
  }, doc);

  return def.promise;
}

function testAdvanceCharsFunction(doc) {
  info("Testing advanceChars as a function");
  let def = promise.defer();

  let firstTime = true;

  createInplaceEditorAndClick({
    initial: "",
    advanceChars: function (charCode, text, insertionPoint) {
      if (charCode !== Components.interfaces.nsIDOMKeyEvent.DOM_VK_COLON) {
        return false;
      }
      if (firstTime) {
        firstTime = false;
        return false;
      }

      // Just to make sure we check it somehow.
      return text.length > 0;
    },
    start: function (editor) {
      for (let ch of ":Test:") {
        EventUtils.sendChar(ch);
      }
    },
    done: onDone(":Test", true, def)
  }, doc);

  return def.promise;
}

function testEscapeCancel(doc) {
  info("Testing that escape cancels the new value");
  let def = promise.defer();

  createInplaceEditorAndClick({
    initial: "initial text",
    start: function (editor) {
      editor.input.value = "Test Value";
      EventUtils.sendKey("escape");
    },
    done: onDone("initial text", false, def)
  }, doc);

  return def.promise;
}

function onDone(value, isCommit, def) {
  return function (actualValue, actualCommit) {
    info("Inplace-editor's done callback executed, checking its state");
    is(actualValue, value, "The value is correct");
    is(actualCommit, isCommit, "The commit boolean is correct");
    def.resolve();
  };
}
