/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URI =
  "chrome://mochitests/content/browser/devtools/client" +
  "/shared/sourceeditor/test/codemirror/vimemacs.html";
loadHelperScript("helper_codemirror_runner.js");

function test() {
  requestLongerTimeout(4);
  waitForExplicitFinish();

  addTab(URI).then(function(tab) {
    runCodeMirrorTest(tab.linkedBrowser);
  });
}
