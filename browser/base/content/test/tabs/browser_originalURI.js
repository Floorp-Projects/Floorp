/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
  These tests ensure the originalURI property of the <browser> element
  has consistent behavior when the URL of a <browser> changes.
*/

const EXAMPLE_URL = "https://example.com/some/path";
const EXAMPLE_URL_2 = "http://mochi.test:8888/";

/*
  Load a page with no redirect.
*/
add_task(async function no_redirect() {
  await BrowserTestUtils.withNewTab(EXAMPLE_URL, async browser => {
    info("Page loaded.");
    assertUrlEqualsOriginalURI(EXAMPLE_URL, browser.originalURI);
  });
});

/*
  Load a page, go to another page, then go back and forth.
*/
add_task(async function back_and_forth() {
  await BrowserTestUtils.withNewTab(EXAMPLE_URL, async browser => {
    info("Page loaded.");

    info("Try loading another page.");
    let pageLoadPromise = BrowserTestUtils.browserLoaded(
      browser,
      false,
      EXAMPLE_URL_2
    );
    BrowserTestUtils.startLoadingURIString(browser, EXAMPLE_URL_2);
    await pageLoadPromise;
    info("Other page finished loading.");
    assertUrlEqualsOriginalURI(EXAMPLE_URL_2, browser.originalURI);

    let pageShowPromise = BrowserTestUtils.waitForContentEvent(
      browser,
      "pageshow"
    );
    browser.goBack();
    info("Go back.");
    await pageShowPromise;

    info("Loaded previous page.");
    assertUrlEqualsOriginalURI(EXAMPLE_URL, browser.originalURI);

    pageShowPromise = BrowserTestUtils.waitForContentEvent(browser, "pageshow");
    browser.goForward();
    info("Go forward.");
    await pageShowPromise;

    info("Loaded next page.");
    assertUrlEqualsOriginalURI(EXAMPLE_URL_2, browser.originalURI);
  });
});

/*
  Redirect using the Location interface.
*/
add_task(async function location_href() {
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    let pageLoadPromise = BrowserTestUtils.browserLoaded(
      browser,
      false,
      EXAMPLE_URL
    );
    info("Loading page with location.href interface.");
    await SpecialPowers.spawn(browser, [EXAMPLE_URL], href => {
      content.document.location.href = href;
    });
    await pageLoadPromise;
    info("Page loaded.");
    assertUrlEqualsOriginalURI(EXAMPLE_URL, browser.originalURI);
  });
});

/*
  Redirect using History API, should not update the originalURI.
*/
add_task(async function push_state() {
  await BrowserTestUtils.withNewTab(EXAMPLE_URL, async browser => {
    info("Page loaded.");

    info("Pushing state via History API.");
    await SpecialPowers.spawn(browser, [], () => {
      let newUrl = content.document.location.href + "/after?page=images";
      content.history.pushState(null, "", newUrl);
    });
    Assert.equal(
      browser.currentURI.displaySpec,
      EXAMPLE_URL + "/after?page=images",
      "Current URI should be modified by push state."
    );
    assertUrlEqualsOriginalURI(EXAMPLE_URL, browser.originalURI);
  });
});

/*
  Redirect using the <meta> tag.
*/
add_task(async function meta_tag() {
  let URL = httpURL("redirect_via_meta_tag.html");
  await BrowserTestUtils.withNewTab(URL, async browser => {
    info("Page loaded.");

    let pageLoadPromise = BrowserTestUtils.browserLoaded(
      browser,
      false,
      EXAMPLE_URL_2
    );
    await pageLoadPromise;
    info("Redirected to ", EXAMPLE_URL_2);
    assertUrlEqualsOriginalURI(URL, browser.originalURI);
  });
});

/*
  Redirect using header from a server.
*/
add_task(async function server_header() {
  let URL = httpURL("redirect_via_header.html");
  await BrowserTestUtils.withNewTab(URL, async browser => {
    info("Page loaded.");

    Assert.equal(
      browser.currentURI.displaySpec,
      EXAMPLE_URL,
      `Browser should be re-directed to ${EXAMPLE_URL}`
    );
    assertUrlEqualsOriginalURI(URL, browser.originalURI);
  });
});

/*
  Load a page with an iFrame and then try having the
  iFrame load another page.
*/
add_task(async function page_with_iframe() {
  let URL = httpURL("page_with_iframe.html");
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    info("Blank page loaded.");

    info("Load URL.");
    BrowserTestUtils.startLoadingURIString(browser, URL);
    // Make sure the iFrame is finished loading.
    await BrowserTestUtils.browserLoaded(
      browser,
      true,
      "https://example.com/another/site"
    );
    info("iFrame finished loading.");
    assertUrlEqualsOriginalURI(URL, browser.originalURI);

    info("Change location of the iframe.");
    let pageLoadPromise = BrowserTestUtils.browserLoaded(
      browser,
      true,
      EXAMPLE_URL_2
    );
    await SpecialPowers.spawn(browser, [EXAMPLE_URL_2], url => {
      content.document.getElementById("hidden-iframe").contentWindow.location =
        url;
    });
    await pageLoadPromise;
    info("iFrame finished loading.");
    assertUrlEqualsOriginalURI(URL, browser.originalURI);
  });
});

function assertUrlEqualsOriginalURI(url, originalURI) {
  let uri = Services.io.newURI(url);
  Assert.ok(
    uri.equals(gBrowser.selectedBrowser.originalURI),
    `URI - ${uri.displaySpec} is not equal to the originalURI - ${originalURI.displaySpec}`
  );
}
