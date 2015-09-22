/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the externalScripts option, which allows custom language modes or
// other scripts to be injected into the editor window.  See Bug 1089428.

"use strict";

add_task(function*() {
  yield runTest();
});

function* runTest() {
  const baseURL = "chrome://mochitests/content/browser/devtools/client/sourceeditor/test"
  const injectedText = "Script successfully injected !";

  let {ed, win} = yield setup(null, {
    mode: "ruby",
    externalScripts: [`${baseURL}/cm_script_injection_test.js`,
                      `${baseURL}/cm_mode_ruby.js`]
  });

  is(ed.getText(), injectedText, "The text has been injected");
  is(ed.getOption("mode"), "ruby", "The ruby mode is correctly set");
  teardown(ed, win);
}