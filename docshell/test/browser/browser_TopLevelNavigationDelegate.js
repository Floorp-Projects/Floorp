/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ACTOR_NAME = "TopLevelNavigationDelegate";
const ACTOR_MODULE_URI =
  getRootDirectory(gTestPath) + "TestTopLevelNavigationDelegate.jsm";
const IFRAME_PAGE =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "http://example.com"
  ) + "dummy_iframe_page.html";

/**
 * Tests that when browser navigates to uriString, that the
 * TopLevelNavigationDelegateChild detects it, prevents the navigation,
 * and that the TopLevelNavigationDelegateParent is notified
 * about the navigation attempt.
 *
 * @param {Element} browser The <xul:browser> to attempt the navigation with.
 * @param {String} uriString The URI to navigate browser to.
 * @return {Promise}
 * @resolves {undefined} Once the delegation has been detected.
 */
async function doesDelegateFor(browser, uriString) {
  let delegatePromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "TopLevelNavigationDelegateEvent",
    false,
    null,
    true
  );
  BrowserTestUtils.loadURI(browser, uriString);
  await delegatePromise;
  Assert.ok(true, `Delegated navigation for ${uriString}`);
  let parentActor = browser.browsingContext.currentWindowGlobal.getActor(
    ACTOR_NAME
  );
  Assert.equal(
    parentActor.seenURIStrings.shift(),
    uriString,
    "TopLevelNavigationDelegateParent saw the load attempt."
  );
}

/**
 * Tests that when browser navigates to uriString, that the
 * TopLevelNavigationDelegateChild does not prevent the navigation,
 * and that the load event properly fires after reaching the new
 * location.
 *
 * @param {Element} browser The <xul:browser> to attempt the navigation with.
 * @param {String} uriString The URI to navigate browser to.
 * @param {Boolean} hashChange The URI to navigate to is a same-document fragment
 *   navigation.
 * @return {Promise}
 * @resolves {undefined} Once the successful load has been detected.
 */
async function doesNotDelegateFor(browser, uriString, hashChange = false) {
  let loaded = hashChange
    ? BrowserTestUtils.waitForContentEvent(browser, "hashchange", true)
    : BrowserTestUtils.browserLoaded(browser, false, uriString);
  BrowserTestUtils.loadURI(browser, uriString);
  await loaded;
  Assert.ok(true, `Successfully navigated to ${uriString}`);
}

/**
 * Registers a test nsITopLevelNavigationDelegate JSWindowActorChild that
 * stops attempts to reach example.org on top-level BrowsingContexts, and
 * then ensures that example.org sites cannot be loaded, but example.com
 * sites can. Also tests that iframes can load both example.org and
 * example.com sites successfully. Finally, unregisters the
 * JSWindowActorChild and ensures that top-level example.org navigations
 * are no longer prevented.
 */
add_task(async function() {
  await BrowserTestUtils.withNewTab("http://example.com", async browser => {
    ChromeUtils.registerWindowActor(ACTOR_NAME, {
      child: {
        moduleURI: ACTOR_MODULE_URI,
      },
      parent: {
        moduleURI: ACTOR_MODULE_URI,
      },
      messageManagerGroups: ["browsers"],
    });

    // This is normally done towards the end of the test, but just in case that
    // task exits early for some reason, we unregister the JSWindowActorChild
    // to ensure we don't contaminate other tests. Doing this when the
    // JSWindowActorChild is already unregistered is a no-op.
    registerCleanupFunction(() => {
      ChromeUtils.unregisterWindowActor(ACTOR_NAME);
    });

    await doesDelegateFor(browser, "http://example.org/");
    await doesDelegateFor(browser, "http://example.org/2");
    // We still want to delegate for a fragment navigation when we're
    // transitioning from example.com to example.org, and is not a
    // same document navigation.
    await doesDelegateFor(browser, "http://example.org/2#test");

    await doesNotDelegateFor(browser, "https://example.com/");
    await doesNotDelegateFor(
      browser,
      "https://example.com/#test",
      true /* hashChange */
    );
    await doesNotDelegateFor(browser, "http://example.com/2");
    await doesNotDelegateFor(browser, "https://example.com/3");

    await doesNotDelegateFor(browser, IFRAME_PAGE);

    // Now check that iframes don't qualify for delegation
    await SpecialPowers.spawn(browser, [], async () => {
      let iframe1 = content.document.getElementById("frame1");
      iframe1.remove();
      iframe1.src = "http://example.com";
      let load1 = ContentTaskUtils.waitForEvent(iframe1, "load");
      content.document.body.appendChild(iframe1);
      await load1;

      let iframe2 = content.document.getElementById("frame2");
      iframe2.remove();
      iframe2.src = "http://example.org";
      let load2 = ContentTaskUtils.waitForEvent(iframe2, "load");
      content.document.body.appendChild(iframe2);
      await load2;
    });

    ChromeUtils.unregisterWindowActor(ACTOR_NAME);

    await doesNotDelegateFor(browser, "http://example.org/2");
    await doesNotDelegateFor(browser, "https://example.org/3");
    await doesNotDelegateFor(browser, "https://example.org/");
    // We expect a same-document fragment navigation to succeed
    // on example.org now.
    await doesNotDelegateFor(
      browser,
      "https://example.org/#test",
      true /* hashChange */
    );
  });
});
