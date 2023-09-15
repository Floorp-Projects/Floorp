/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const ROOT_DIR = getRootDirectory(gTestPath);

const MOCHI_ROOT = ROOT_DIR.replace(
  "chrome://mochitests/content/",
  "http://mochi.test:8888/"
);

const EXAMPLE_NET_ROOT = ROOT_DIR.replace(
  "chrome://mochitests/content/",
  "http://example.net/"
);

const EXAMPLE_COM_ROOT = ROOT_DIR.replace(
  "chrome://mochitests/content/",
  "http://example.com/"
);

const FAVICON_URL = EXAMPLE_COM_ROOT + "crossorigin.png";

function run_test(root, shouldSucceed, description) {
  add_task(async () => {
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: "about:blank" },
      async browser => {
        const faviconPromise = waitForFaviconMessage(true, FAVICON_URL);

        BrowserTestUtils.startLoadingURIString(
          browser,
          `${root}crossorigin.html`
        );
        await BrowserTestUtils.browserLoaded(browser);

        if (shouldSucceed) {
          try {
            const result = await faviconPromise;
            Assert.equal(
              result.iconURL,
              FAVICON_URL,
              `Should have seen the icon (${description}).`
            );
          } catch (e) {
            Assert.ok(false, `Favicon load failed (${description}).`);
          }
        } else {
          await Assert.rejects(
            faviconPromise,
            result => result.iconURL == FAVICON_URL,
            `Should have failed to load the icon (${description}).`
          );
        }
      }
    );
  });
}

// crossorigin.png only allows CORS for MOCHI_ROOT.
run_test(EXAMPLE_NET_ROOT, false, "remote origin not allowed");
run_test(MOCHI_ROOT, true, "remote origin allowed");

// Same-origin but with the crossorigin attribute.
run_test(EXAMPLE_COM_ROOT, true, "same-origin");
