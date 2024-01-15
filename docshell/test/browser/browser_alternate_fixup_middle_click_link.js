/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that we don't do alternate fixup when users middle-click
 * broken links in the content document.
 */
add_task(async function test_alt_fixup_middle_click() {
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    await SpecialPowers.spawn(browser, [], () => {
      let link = content.document.createElement("a");
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      link.href = "http://example/foo";
      link.textContent = "Me, me, click me!";
      content.document.body.append(link);
    });
    let newTabPromise = BrowserTestUtils.waitForNewTab(
      gBrowser,
      /* wantLoad = */ null,
      /* waitForLoad = */ true,
      /* waitForAnyTab = */ false,
      /* maybeErrorPage = */ true
    );
    await BrowserTestUtils.synthesizeMouseAtCenter(
      "a[href]",
      { button: 1 },
      browser
    );
    let tab = await newTabPromise;
    let { browsingContext } = tab.linkedBrowser;
    // TBH, if the test fails, we probably force-crash because we try to reach
    // *www.* example.com, which isn't proxied by the test infrastructure so
    // will forcibly abort the test. But we need some asserts so they might as
    // well be meaningful:
    is(
      tab.linkedBrowser.currentURI.spec,
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      "http://example/foo",
      "URL for tab should be correct."
    );

    ok(
      browsingContext.currentWindowGlobal.documentURI.spec.startsWith(
        "about:neterror"
      ),
      "Should be showing error page."
    );
    BrowserTestUtils.removeTab(tab);
  });
});
