/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  const URI = "data:text/html;charset=utf-8,<iframe id='test-iframe'></iframe>";

  await BrowserTestUtils.withNewTab({ gBrowser, url: URI }, async function(
    browser
  ) {
    await SpecialPowers.spawn(browser, [], test_init);

    browser.browsingContext.touchEventsOverride = "disabled";

    await SpecialPowers.spawn(browser, [], test_body);
  });
});

async function test_init() {
  is(
    content.browsingContext.touchEventsOverride,
    "none",
    "touchEventsOverride flag should be initially set to NONE"
  );
}

async function test_body() {
  let bc = content.browsingContext;
  is(
    bc.touchEventsOverride,
    "disabled",
    "touchEventsOverride flag should be changed to DISABLED"
  );

  let frameWin = content.document.querySelector("#test-iframe").contentWindow;
  bc = frameWin.browsingContext;
  is(
    bc.touchEventsOverride,
    "disabled",
    "touchEventsOverride flag should be passed on to frames."
  );

  let newFrame = content.document.createElement("iframe");
  content.document.body.appendChild(newFrame);

  let newFrameWin = newFrame.contentWindow;
  bc = newFrameWin.browsingContext;
  is(
    bc.touchEventsOverride,
    "disabled",
    "Newly created frames should use the new touchEventsOverride flag"
  );

  // Wait for the non-transient about:blank to load.
  await ContentTaskUtils.waitForEvent(newFrame, "load");
  newFrameWin = newFrame.contentWindow;
  bc = newFrameWin.browsingContext;
  is(
    bc.touchEventsOverride,
    "disabled",
    "Newly created frames should use the new touchEventsOverride flag"
  );

  newFrameWin.location.reload();
  await ContentTaskUtils.waitForEvent(newFrame, "load");
  newFrameWin = newFrame.contentWindow;
  bc = newFrameWin.browsingContext;
  is(
    bc.touchEventsOverride,
    "disabled",
    "New touchEventsOverride flag should persist across reloads"
  );
}
