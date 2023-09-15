/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const ROOT_DIR = getRootDirectory(gTestPath);

const EXAMPLE_NET_ROOT = ROOT_DIR.replace(
  "chrome://mochitests/content/",
  "https://example.net/"
);

const EXAMPLE_COM_ROOT = ROOT_DIR.replace(
  "chrome://mochitests/content/",
  "https://example.com/"
);

const FAVICON_URL = EXAMPLE_COM_ROOT + "credentials.png";

// Bug 1746646: Make mochitests work with TCP enabled (cookieBehavior = 5)
// All instances of addPermission and removePermission set up 3rd-party storage
// access in a way that allows the test to proceed with TCP enabled.

function run_test(url, shouldHaveCookies, description) {
  add_task(async () => {
    await SpecialPowers.addPermission(
      "3rdPartyStorage^https://example.com",
      true,
      url
    );

    await BrowserTestUtils.withNewTab(
      { gBrowser, url: "about:blank" },
      async browser => {
        const faviconPromise = waitForFaviconMessage(true, FAVICON_URL);

        BrowserTestUtils.startLoadingURIString(browser, url);
        await BrowserTestUtils.browserLoaded(browser);

        await faviconPromise;

        const seenCookie = Services.cookies
          .getCookiesFromHost(
            "example.com", // the icon's host, not the page's
            browser.contentPrincipal.originAttributes
          )
          .some(cookie => cookie.name == "faviconCookie2");

        // Clean up.
        Services.cookies.removeAll();
        Services.cache2.clear();

        if (shouldHaveCookies) {
          Assert.ok(
            seenCookie,
            `Should have seen the cookie (${description}).`
          );
        } else {
          Assert.ok(
            !seenCookie,
            `Should have not seen the cookie (${description}).`
          );
        }
      }
    );
    await SpecialPowers.removePermission(
      "3rdPartyStorage^https://example.com",
      url
    );
  });
}

// crossorigin="" only has credentials in the same-origin case
run_test(`${EXAMPLE_NET_ROOT}credentials1.html`, false, "anonymous, remote");
run_test(
  `${EXAMPLE_COM_ROOT}credentials1.html`,
  true,
  "anonymous, same-origin"
);

// crossorigin="use-credentials" always has them
run_test(
  `${EXAMPLE_NET_ROOT}credentials2.html`,
  true,
  "use-credentials, remote"
);
run_test(
  `${EXAMPLE_COM_ROOT}credentials2.html`,
  true,
  "use-credentials, same-origin"
);
