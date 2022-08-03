/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_inplace_editor.js */

"use strict";

loadHelperScript("helper_inplace_editor.js");

// Test that the trimOutput option for the inplace editor works correctly.

add_task(async function() {
  await addTab("data:text/html;charset=utf-8,inline editor tests");
  const { host, doc } = await createHost();

  await testNonTrimmed(doc);
  await testTrimmed(doc);

  host.destroy();
  gBrowser.removeCurrentTab();
});

function testNonTrimmed(doc) {
  info("Testing the trimOutput=false option");
  return new Promise(resolve => {
    const initial = "\nMultiple\nLines\n";
    const changed = " \nMultiple\nLines\n with more whitespace ";
    createInplaceEditorAndClick(
      {
        trimOutput: false,
        multiline: true,
        initial,
        start(editor) {
          is(
            editor.input.value,
            initial,
            "Explicit initial value should be used."
          );
          editor.input.value = changed;
          EventUtils.sendKey("return");
        },
        done: onDone(changed, true, resolve),
      },
      doc
    );
  });
}

function testTrimmed(doc) {
  info("Testing the trimOutput=true option (default value)");
  return new Promise(resolve => {
    const initial = "\nMultiple\nLines\n";
    const changed = " \nMultiple\nLines\n with more whitespace ";
    createInplaceEditorAndClick(
      {
        initial,
        multiline: true,
        start(editor) {
          is(
            editor.input.value,
            initial,
            "Explicit initial value should be used."
          );
          editor.input.value = changed;
          EventUtils.sendKey("return");
        },
        done: onDone(changed.trim(), true, resolve),
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
