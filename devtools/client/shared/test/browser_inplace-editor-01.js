/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_inplace_editor.js */

"use strict";

loadHelperScript("helper_inplace_editor.js");

// Test the inplace-editor behavior.

add_task(async function() {
  await addTab("data:text/html;charset=utf-8,inline editor tests");
  const { host, doc } = await createHost();

  await testMultipleInitialization(doc);
  await testReturnCommit(doc);
  await testBlurCommit(doc);
  await testAdvanceCharCommit(doc);
  await testAdvanceCharsFunction(doc);
  await testEscapeCancel(doc);

  host.destroy();
  gBrowser.removeCurrentTab();
});

function testMultipleInitialization(doc) {
  doc.body.innerHTML = "";
  const options = {};
  const span = (options.element = createSpan(doc));

  info("Creating multiple inplace-editor fields");
  editableField(options);
  editableField(options);

  info("Clicking on the inplace-editor field to turn to edit mode");
  span.click();

  is(span.style.display, "none", "The original <span> is hidden");
  is(doc.querySelectorAll("input").length, 1, "Only one <input>");
  is(
    doc.querySelectorAll("span").length,
    2,
    "Correct number of <span> elements"
  );
  is(
    doc.querySelectorAll("span.autosizer").length,
    1,
    "There is an autosizer element"
  );
}

function testReturnCommit(doc) {
  info("Testing that pressing return commits the new value");
  return new Promise(resolve => {
    createInplaceEditorAndClick(
      {
        initial: "explicit initial",
        start(editor) {
          is(
            editor.input.value,
            "explicit initial",
            "Explicit initial value should be used."
          );
          editor.input.value = "Test Value";
          EventUtils.sendKey("return");
        },
        done: onDone("Test Value", true, resolve),
      },
      doc
    );
  });
}

function testBlurCommit(doc) {
  info("Testing that bluring the field commits the new value");
  return new Promise(resolve => {
    createInplaceEditorAndClick(
      {
        start(editor) {
          is(editor.input.value, "Edit Me!", "textContent of the span used.");
          editor.input.value = "Test Value";
          editor.input.blur();
        },
        done: onDone("Test Value", true, resolve),
      },
      doc,
      "Edit Me!"
    );
  });
}

function testAdvanceCharCommit(doc) {
  info("Testing that configured advanceChars commit the new value");
  return new Promise(resolve => {
    createInplaceEditorAndClick(
      {
        advanceChars: ":",
        start(editor) {
          EventUtils.sendString("Test:");
        },
        done: onDone("Test", true, resolve),
      },
      doc
    );
  });
}

function testAdvanceCharsFunction(doc) {
  info("Testing advanceChars as a function");
  return new Promise(resolve => {
    let firstTime = true;

    createInplaceEditorAndClick(
      {
        initial: "",
        advanceChars(charCode, text, insertionPoint) {
          if (charCode !== KeyboardEvent.DOM_VK_COLON) {
            return false;
          }
          if (firstTime) {
            firstTime = false;
            return false;
          }

          // Just to make sure we check it somehow.
          return !!text.length;
        },
        start(editor) {
          for (const ch of ":Test:") {
            EventUtils.sendChar(ch);
          }
        },
        done: onDone(":Test", true, resolve),
      },
      doc
    );
  });
}

function testEscapeCancel(doc) {
  info("Testing that escape cancels the new value");
  return new Promise(resolve => {
    createInplaceEditorAndClick(
      {
        initial: "initial text",
        start(editor) {
          editor.input.value = "Test Value";
          EventUtils.sendKey("escape");
        },
        done: onDone("initial text", false, resolve),
      },
      doc
    );
  });
}

function onDone(value, isCommit, resolve) {
  return function(actualValue, actualCommit) {
    info("Inplace-editor's done callback executed, checking its state");
    is(actualValue, value, "The value is correct");
    is(actualCommit, isCommit, "The commit boolean is correct");
    resolve();
  };
}
