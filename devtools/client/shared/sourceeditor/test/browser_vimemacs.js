/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URI =
  "chrome://mochitests/content/browser/devtools/client" +
  "/shared/sourceeditor/test/codemirror/vimemacs.html";
loadHelperScript("helper_codemirror_runner.js");

async function test() {
  requestLongerTimeout(4);
  waitForExplicitFinish();

  // Needed for a loadFrameScript(data:) call in helper_codemirror_runner.js
  await pushPref("security.allow_parent_unrestricted_js_loads", true);

  const tab = await addTab(URI);
  runCodeMirrorTest(tab.linkedBrowser);
}
