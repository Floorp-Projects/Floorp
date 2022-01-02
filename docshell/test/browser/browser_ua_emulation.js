/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = "data:text/html;charset=utf-8,<iframe id='test-iframe'></iframe>";

// Test that the docShell UA emulation works
async function contentTaskNoOverride() {
  let docshell = docShell;
  is(
    docshell.browsingContext.customUserAgent,
    "",
    "There should initially be no customUserAgent"
  );
}

async function contentTaskOverride() {
  let docshell = docShell;
  is(
    docshell.browsingContext.customUserAgent,
    "foo",
    "The user agent should be changed to foo"
  );

  is(
    content.navigator.userAgent,
    "foo",
    "The user agent should be changed to foo"
  );

  let frameWin = content.document.querySelector("#test-iframe").contentWindow;
  is(
    frameWin.navigator.userAgent,
    "foo",
    "The UA should be passed on to frames."
  );

  let newFrame = content.document.createElement("iframe");
  content.document.body.appendChild(newFrame);

  let newFrameWin = newFrame.contentWindow;
  is(
    newFrameWin.navigator.userAgent,
    "foo",
    "Newly created frames should use the new UA"
  );

  newFrameWin.location.reload();
  await ContentTaskUtils.waitForEvent(newFrame, "load");

  is(
    newFrameWin.navigator.userAgent,
    "foo",
    "New UA should persist across reloads"
  );
}

add_task(async function() {
  await BrowserTestUtils.withNewTab({ gBrowser, url: URL }, async function(
    browser
  ) {
    await SpecialPowers.spawn(browser, [], contentTaskNoOverride);

    let browsingContext = BrowserTestUtils.getBrowsingContextFrom(browser);
    browsingContext.customUserAgent = "foo";

    await SpecialPowers.spawn(browser, [], contentTaskOverride);
  });
});
