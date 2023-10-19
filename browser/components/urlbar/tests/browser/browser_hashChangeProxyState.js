/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Check that navigating through both the URL bar and using in-page hash- or ref-
 * based links and back or forward navigation updates the URL bar and identity block correctly.
 */
add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.navigation.requireUserInteraction", false]],
  });
  let baseURL = `${TEST_BASE_URL}dummy_page.html`;
  let url = baseURL + "#foo";
  await BrowserTestUtils.withNewTab(
    { gBrowser, url },
    async function (browser) {
      let identityBox = document.getElementById("identity-box");
      let expectedURL = url;

      let verifyURLBarState = testType => {
        is(
          gURLBar.value,
          UrlbarTestUtils.trimURL(expectedURL),
          "URL bar visible value should be correct " + testType
        );
        is(
          gURLBar.untrimmedValue,
          expectedURL,
          "URL bar value should be correct " + testType
        );
        ok(
          identityBox.classList.contains("verifiedDomain"),
          "Identity box should know we're doing SSL " + testType
        );
        is(
          gURLBar.getAttribute("pageproxystate"),
          "valid",
          "URL bar is in valid page proxy state"
        );
      };

      verifyURLBarState("at the beginning");

      let locationChangePromise;
      let resolveLocationChangePromise;
      let expectURL = urlTemp => {
        expectedURL = urlTemp;
        locationChangePromise = new Promise(
          r => (resolveLocationChangePromise = r)
        );
      };
      let wpl = {
        onLocationChange(unused, unused2, location) {
          is(location.spec, expectedURL, "Got the expected URL");
          resolveLocationChangePromise();
        },
      };
      gBrowser.addProgressListener(wpl);

      expectURL(baseURL + "#foo");
      gURLBar.select();
      EventUtils.sendKey("return");

      await locationChangePromise;
      verifyURLBarState("after hitting enter on the same URL a second time");

      expectURL(baseURL + "#bar");
      gURLBar.value = expectedURL;
      gURLBar.select();
      EventUtils.sendKey("return");

      await locationChangePromise;
      verifyURLBarState("after a URL bar hash navigation");

      expectURL(baseURL + "#foo");
      await SpecialPowers.spawn(browser, [], function () {
        let a = content.document.createElement("a");
        a.href = "#foo";
        a.textContent = "Foo Link";
        content.document.body.appendChild(a);
        a.click();
      });

      await locationChangePromise;
      verifyURLBarState("after a page link hash navigation");

      expectURL(baseURL + "#bar");
      gBrowser.goBack();

      await locationChangePromise;
      verifyURLBarState("after going back");

      expectURL(baseURL + "#foo");
      gBrowser.goForward();

      await locationChangePromise;
      verifyURLBarState("after going forward");

      expectURL(baseURL + "#foo");
      gURLBar.select();
      EventUtils.sendKey("return");

      await locationChangePromise;
      verifyURLBarState("after hitting enter on the same URL");

      gBrowser.removeProgressListener(wpl);
    }
  );
});

/**
 * Check that initial secure loads that swap remoteness
 * get the correct page icon when finished.
 */
add_task(async function () {
  // Ensure there's no preloaded newtab browser, since that'll not fire a load event.
  NewTabPagePreloading.removePreloadedBrowser(window);
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:newtab"
  );
  let url = `${TEST_BASE_URL}dummy_page.html#foo`;
  gURLBar.value = url;
  gURLBar.select();
  EventUtils.sendKey("return");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  is(
    gURLBar.value,
    UrlbarTestUtils.trimURL(url),
    "URL bar visible value should be correct when the page loads from about:newtab"
  );
  is(
    gURLBar.untrimmedValue,
    url,
    "URL bar value should be correct when the page loads from about:newtab"
  );
  let identityBox = document.getElementById("identity-box");
  ok(
    identityBox.classList.contains("verifiedDomain"),
    "Identity box should know we're doing SSL when the page loads from about:newtab"
  );
  is(
    gURLBar.getAttribute("pageproxystate"),
    "valid",
    "URL bar is in valid page proxy state when SSL page with hash loads from about:newtab"
  );
  BrowserTestUtils.removeTab(tab);
});
