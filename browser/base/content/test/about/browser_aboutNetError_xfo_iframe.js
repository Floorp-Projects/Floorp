/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BLOCKED_PAGE =
  // eslint-disable-next-line @microsoft/sdl/no-insecure-url
  "http://example.org:8000/browser/browser/base/content/test/about/xfo_iframe.sjs";

add_task(async function test_xfo_iframe() {
  let { iframePageTab, blockedPageTab } = await setupPage(
    "iframe_page_xfo.html",
    BLOCKED_PAGE
  );

  let xfoBrowser = gBrowser.selectedTab.linkedBrowser;

  // The blocked page opened in a new window/tab
  await SpecialPowers.spawn(
    xfoBrowser,
    [BLOCKED_PAGE],
    async function (xfoBlockedPage) {
      let cookieHeader = content.document.getElementById("strictCookie");
      let location = content.document.location.href;

      Assert.ok(
        cookieHeader.textContent.includes("No same site strict cookie header"),
        "Same site strict cookie has not been set"
      );
      Assert.equal(
        location,
        xfoBlockedPage,
        "Location of new page is correct!"
      );
    }
  );

  Services.cookies.removeAll();
  BrowserTestUtils.removeTab(iframePageTab);
  BrowserTestUtils.removeTab(blockedPageTab);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

async function setupPage(htmlPageName, blockedPage) {
  let iFramePage =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      "http://example.com"
    ) + htmlPageName;

  // Opening the blocked page once in a new tab
  let blockedPageTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    blockedPage
  );
  let blockedPageBrowser = blockedPageTab.linkedBrowser;

  let cookies = Services.cookies.getCookiesFromHost(
    "example.org",
    blockedPageBrowser.contentPrincipal.originAttributes
  );
  let strictCookie = cookies[0];

  is(
    strictCookie.value,
    "creamy",
    "Same site strict cookie has the expected value"
  );

  is(strictCookie.sameSite, 2, "The cookie is a same site strict cookie");

  // Opening the page that contains the iframe
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  let browser = tab.linkedBrowser;
  let browserLoaded = BrowserTestUtils.browserLoaded(
    browser,
    true,
    blockedPage,
    true
  );

  BrowserTestUtils.startLoadingURIString(browser, iFramePage);
  await browserLoaded;
  info("The error page has loaded!");

  await SpecialPowers.spawn(browser, [], async function () {
    let iframe = content.document.getElementById("theIframe");

    await ContentTaskUtils.waitForCondition(() =>
      SpecialPowers.spawn(iframe, [], () =>
        content.document.body.classList.contains("neterror")
      )
    );
  });

  let frameContext = browser.browsingContext.children[0];
  let newTabLoaded = BrowserTestUtils.waitForNewTab(gBrowser, null, true);

  // In the iframe, we see the correct error page and click on the button
  // to open the blocked page in a new window/tab
  await SpecialPowers.spawn(frameContext, [], async function () {
    let doc = content.document;
    let textLongDescription = doc.getElementById("errorLongDesc").textContent;
    let learnMoreLinkLocation = doc.getElementById("learnMoreLink").href;

    Assert.ok(
      textLongDescription.includes(
        "To see this page, you need to open it in a new window."
      ),
      "Correct error message found"
    );

    let button = doc.getElementById("openInNewWindowButton");
    Assert.ok(
      button.textContent.includes("Open Site in New Window"),
      "We see the correct button to open the site in a new window"
    );

    Assert.ok(
      learnMoreLinkLocation.includes("xframe-neterror-page"),
      "Correct Learn More URL for XFO error page"
    );

    // We click on the button
    await EventUtils.synthesizeMouseAtCenter(button, {}, content);
  });
  info("Button was clicked!");

  // We wait for the new tab to load
  await newTabLoaded;
  info("The new tab has loaded!");

  let iframePageTab = tab;
  return {
    iframePageTab,
    blockedPageTab,
  };
}
