"use strict";

add_task(async function () {
  const outer =
    "http://mochi.test:8888/browser/docshell/test/browser/file_onbeforeunload_nested_outer.html";
  await BrowserTestUtils.withNewTab(outer, async browser => {
    let inner = browser.browsingContext.children[0];

    // Install a beforeunload event handler that resolves a promise.
    await SpecialPowers.spawn(inner, [], () => {
      let { promise, resolve } = Promise.withResolvers();
      content.addEventListener(
        "beforeunload",
        e => {
          e.preventDefault();
          resolve();
        },
        {
          once: true,
        }
      );
      content.beforeunloadPromise = promise;
    });

    // Get the promise for the beforeunload handler so we can wait for it.
    let beforeunloadFired = SpecialPowers.spawn(
      browser.browsingContext.children[0],
      [],
      () => {
        return content.beforeunloadPromise;
      }
    );

    // Register whether a load has started.
    let loaded = BrowserTestUtils.browserLoaded(browser).then(() => "loaded");

    // This is rather fragile, but we need to make sure we don't start a
    // speculative load in the parent so let's give it the time to be done.
    let timeout = new Promise(resolve =>
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      setTimeout(() => resolve("timeout"), 3000)
    );

    // Need to add some user interaction for beforeunload.
    await BrowserTestUtils.synthesizeMouse("body", 0, 0, {}, inner, true);

    // Try to start a speculative load in the parent.
    BrowserTestUtils.startLoadingURIString(browser, "https://example.com/");

    await beforeunloadFired;

    is(
      await Promise.race([loaded, timeout]),
      "timeout",
      "Timed out because the load was blocked by beforeunload."
    );
  });
});
