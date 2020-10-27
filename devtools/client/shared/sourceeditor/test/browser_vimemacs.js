/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URI =
  "chrome://mochitests/content/browser/devtools/client" +
  "/shared/sourceeditor/test/codemirror/vimemacs.html";

add_task(async function test() {
  requestLongerTimeout(4);
  await runCodeMirrorTest(URI);
});
