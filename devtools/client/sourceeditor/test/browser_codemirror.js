/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URI = "chrome://mochitests/content/browser/devtools/client/" +
            "sourceeditor/test/codemirror/codemirror.html";
loadHelperScript("helper_codemirror_runner.js");

function test() {
  requestLongerTimeout(3);
  waitForExplicitFinish();

  addTab(URI).then(function (tab) {
    runCodeMirrorTest(tab.linkedBrowser);
  });
}
