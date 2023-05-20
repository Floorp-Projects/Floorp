/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const URL = "data:text/html;charset=utf-8,<iframe id='test-iframe'></iframe>";

async function contentTaskNoOverride() {
  let docshell = docShell;
  is(
    docshell.browsingContext.customPlatform,
    "",
    "There should initially be no customPlatform"
  );
}

async function contentTaskOverride() {
  let docshell = docShell;
  is(
    docshell.browsingContext.customPlatform,
    "foo",
    "The platform should be changed to foo"
  );

  is(
    content.navigator.platform,
    "foo",
    "The platform should be changed to foo"
  );

  let frameWin = content.document.querySelector("#test-iframe").contentWindow;
  is(
    frameWin.navigator.platform,
    "foo",
    "The platform should be passed on to frames."
  );

  let newFrame = content.document.createElement("iframe");
  content.document.body.appendChild(newFrame);

  let newFrameWin = newFrame.contentWindow;
  is(
    newFrameWin.navigator.platform,
    "foo",
    "Newly created frames should use the new platform"
  );

  newFrameWin.location.reload();
  await ContentTaskUtils.waitForEvent(newFrame, "load");

  is(
    newFrameWin.navigator.platform,
    "foo",
    "New platform should persist across reloads"
  );
}

add_task(async function () {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: URL },
    async function (browser) {
      await SpecialPowers.spawn(browser, [], contentTaskNoOverride);

      let browsingContext = BrowserTestUtils.getBrowsingContextFrom(browser);
      browsingContext.customPlatform = "foo";

      await SpecialPowers.spawn(browser, [], contentTaskOverride);
    }
  );
});
