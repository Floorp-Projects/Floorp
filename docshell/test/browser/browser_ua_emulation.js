/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = "data:text/html;charset=utf-8,<iframe id='test-iframe'></iframe>";

// Test that the docShell UA emulation works
function* contentTask() {
  let docshell = docShell;
  is(docshell.customUserAgent, "", "There should initially be no customUserAgent");

  docshell.customUserAgent = "foo";
  is(content.navigator.userAgent, "foo", "The user agent should be changed to foo");

  let frameWin = content.document.querySelector("#test-iframe").contentWindow;
  is(frameWin.navigator.userAgent, "foo", "The UA should be passed on to frames.");

  let newFrame = content.document.createElement("iframe");
  content.document.body.appendChild(newFrame);

  let newFrameWin = newFrame.contentWindow;
  is(newFrameWin.navigator.userAgent, "foo", "Newly created frames should use the new UA");

  newFrameWin.location.reload();
  yield ContentTaskUtils.waitForEvent(newFrameWin, "load");

  is(newFrameWin.navigator.userAgent, "foo", "New UA should persist across reloads");
}

add_task(function* () {
  yield BrowserTestUtils.withNewTab({ gBrowser, url: URL },
    function* (browser) {
      yield ContentTask.spawn(browser, null, contentTask);
    });
});
