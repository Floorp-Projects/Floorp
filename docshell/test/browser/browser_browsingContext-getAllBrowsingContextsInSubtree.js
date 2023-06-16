/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function addFrame(url) {
  let iframe = content.document.createElement("iframe");
  await new Promise(resolve => {
    iframe.addEventListener("load", resolve, { once: true });
    iframe.src = url;
    content.document.body.appendChild(iframe);
  });
  return iframe.browsingContext;
}

add_task(async function () {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:blank" },
    async browser => {
      // Add 15 example.com frames to the toplevel document.
      let frames = await Promise.all(
        Array.from({ length: 15 }).map(_ =>
          // eslint-disable-next-line @microsoft/sdl/no-insecure-url
          SpecialPowers.spawn(browser, ["http://example.com/"], addFrame)
        )
      );

      // Add an example.org subframe to each example.com frame.
      let subframes = await Promise.all(
        Array.from({ length: 15 }).map((_, i) =>
          // eslint-disable-next-line @microsoft/sdl/no-insecure-url
          SpecialPowers.spawn(frames[i], ["http://example.org/"], addFrame)
        )
      );

      Assert.deepEqual(
        subframes[0].getAllBrowsingContextsInSubtree(),
        [subframes[0]],
        "Childless context only has self in subtree"
      );
      Assert.deepEqual(
        frames[0].getAllBrowsingContextsInSubtree(),
        [frames[0], subframes[0]],
        "Single-child context has 2 contexts in subtree"
      );
      Assert.deepEqual(
        browser.browsingContext.getAllBrowsingContextsInSubtree(),
        [browser.browsingContext, ...frames, ...subframes],
        "Toplevel context has all subtree contexts"
      );
    }
  );
});
