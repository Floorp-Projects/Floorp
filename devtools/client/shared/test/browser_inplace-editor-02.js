/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {editableField, getInplaceEditorForSpan: inplaceEditor} = require("devtools/client/shared/inplace-editor");

// Test that the trimOutput option for the inplace editor works correctly.

add_task(function*() {
  yield addTab("data:text/html;charset=utf-8,inline editor tests");
  let [host, win, doc] = yield createHost();

  yield testNonTrimmed(doc);
  yield testTrimmed(doc);

  host.destroy();
  gBrowser.removeCurrentTab();
});

function testNonTrimmed(doc) {
  info("Testing the trimOutput=false option");
  let def = promise.defer();

  let initial = "\nMultiple\nLines\n";
  let changed = " \nMultiple\nLines\n with more whitespace ";
  createInplaceEditorAndClick({
    trimOutput: false,
    multiline: true,
    initial: initial,
    start: function(editor) {
      is(editor.input.value, initial, "Explicit initial value should be used.");
      editor.input.value = changed;
      EventUtils.sendKey("return");
    },
    done: onDone(changed, true, def)
  }, doc);

  return def.promise;
}

function testTrimmed(doc) {
  info("Testing the trimOutput=true option (default value)");
  let def = promise.defer();

  let initial = "\nMultiple\nLines\n";
  let changed = " \nMultiple\nLines\n with more whitespace ";
  createInplaceEditorAndClick({
    initial: initial,
    multiline: true,
    start: function(editor) {
      is(editor.input.value, initial, "Explicit initial value should be used.");
      editor.input.value = changed;
      EventUtils.sendKey("return");
    },
    done: onDone(changed.trim(), true, def)
  }, doc);

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

function createInplaceEditorAndClick(options, doc) {
  doc.body.innerHTML = "";
  let span = options.element = createSpan(doc);

  info("Creating an inplace-editor field");
  editableField(options);

  info("Clicking on the inplace-editor field to turn to edit mode");
  span.click();
}

function createSpan(doc) {
  info("Creating a new span element");
  let span = doc.createElement("span");
  span.setAttribute("tabindex", "0");
  span.textContent = "Edit Me!";
  doc.body.appendChild(span);
  return span;
}
