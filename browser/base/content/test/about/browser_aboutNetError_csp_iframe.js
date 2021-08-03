/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BLOCKED_PAGE =
  "http://example.org:8000/browser/browser/base/content/test/about/csp_iframe.sjs";

add_task(async function test_csp() {
  let { iframePageTab, blockedPageTab } = await setupPage(
    "iframe_page_csp.html",
    BLOCKED_PAGE
  );

  let cspBrowser = gBrowser.selectedTab.linkedBrowser;

  // The blocked page opened in a new window/tab
  await SpecialPowers.spawn(cspBrowser, [BLOCKED_PAGE], async function(
    cspBlockedPage
  ) {
    let cookieHeader = content.document.getElementById("strictCookie");
    let location = content.document.location.href;

    Assert.ok(
      cookieHeader.textContent.includes("No same site strict cookie header"),
      "Same site strict cookie has not been set"
    );
    Assert.equal(location, cspBlockedPage, "Location of new page is correct!");
  });

  Services.cookies.removeAll();
  BrowserTestUtils.removeTab(iframePageTab);
  BrowserTestUtils.removeTab(blockedPageTab);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

async function setupPage(htmlPageName, blockedPage) {
  let iFramePage =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
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
    "green",
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

  BrowserTestUtils.loadURI(browser, iFramePage);
  await browserLoaded;
  info("The error page has loaded!");

  await SpecialPowers.spawn(browser, [], async function() {
    let iframe = content.document.getElementById("theIframe");

    await ContentTaskUtils.waitForCondition(() =>
      SpecialPowers.spawn(iframe, [], () =>
        content.document.body.classList.contains("neterror")
      )
    );
  });

  let iframe = browser.browsingContext.children[0];

  let newTabLoaded = BrowserTestUtils.waitForNewTab(gBrowser, null, true);

  // In the iframe, we see the correct error page and click on the button
  // to open the blocked page in a new window/tab
  await SpecialPowers.spawn(iframe, [], async function() {
    let doc = content.document;

    // aboutNetError.js is using async localization to format several messages
    // and in result the translation may be applied later.
    // We want to return the textContent of the element only after
    // the translation completes, so let's wait for it here.
    let elements = [
      doc.getElementById("errorLongDesc"),
      doc.getElementById("openInNewWindowButton"),
    ];
    await ContentTaskUtils.waitForCondition(() => {
      return elements.every(elem => !!elem.textContent.trim().length);
    });

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
      "Correct Learn More URL for CSP error page"
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
