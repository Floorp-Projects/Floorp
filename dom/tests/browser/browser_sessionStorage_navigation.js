"use strict";

const DIRPATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  ""
);
const PATH = DIRPATH + "file_empty.html";

const ORIGIN1 = "http://example.com";
const ORIGIN2 = "http://example.org";
const URL1 = `${ORIGIN1}/${PATH}`;
const URL2 = `${ORIGIN2}/${PATH}`;

add_task(async function() {
  await BrowserTestUtils.withNewTab(URL1, async function(browser) {
    const key = "key";
    const value = "value";

    info(
      `Verifying sessionStorage is preserved after navigating to a ` +
        `cross-origin site and then navigating back`
    );

    BrowserTestUtils.loadURI(browser, URL1);
    await BrowserTestUtils.browserLoaded(browser);

    await SpecialPowers.spawn(
      browser,
      [ORIGIN1, key, value],
      async (ORIGIN, key, value) => {
        is(content.window.origin, ORIGIN, `Navigate to ${ORIGIN} as expected`);

        let value1 = content.window.sessionStorage.getItem(key);
        is(
          value1,
          null,
          `SessionStroage for ${key} in  ${content.window.origin} is null since` +
            ` it's the first visit`
        );

        content.window.sessionStorage.setItem(key, value);

        let value2 = content.window.sessionStorage.getItem(key);
        is(
          value2,
          value,
          `SessionStorage for ${key} in ${content.window.origin} is set correctly`
        );
      }
    );

    BrowserTestUtils.loadURI(browser, URL2);
    await BrowserTestUtils.browserLoaded(browser);

    await SpecialPowers.spawn(
      browser,
      [ORIGIN2, key, value],
      async (ORIGIN, key, value) => {
        is(content.window.origin, ORIGIN, `Navigate to ${ORIGIN} as expected`);

        let value1 = content.window.sessionStorage.getItem(key);
        is(
          value1,
          null,
          `SessionStroage for ${key} in ${content.window.origin} is null ` +
            `since it's the first visit`
        );
      }
    );

    BrowserTestUtils.loadURI(browser, URL1);
    await BrowserTestUtils.browserLoaded(browser);

    await SpecialPowers.spawn(
      browser,
      [ORIGIN1, key, value],
      async (ORIGIN, key, value) => {
        is(content.window.origin, ORIGIN, `Navigate to ${ORIGIN} as expected`);

        let value1 = content.window.sessionStorage.getItem(key);
        is(
          value1,
          value,
          `SessionStorage for ${key} in ${content.window.origin} is preserved`
        );
      }
    );

    info(`Verifying sessionStorage is preserved for ${URL1} after navigating`);

    BrowserTestUtils.loadURI(browser, URL2);
    await BrowserTestUtils.browserLoaded(browser);

    await SpecialPowers.spawn(
      browser,
      [ORIGIN2, ORIGIN1, URL1, key, value],
      async (ORIGIN, iframeORIGIN, iframeURL, key, value) => {
        is(content.window.origin, ORIGIN, `Navigate to ${ORIGIN} as expected`);

        let iframe = content.document.createElement("iframe");
        iframe.src = iframeURL;
        content.document.body.appendChild(iframe);
        await ContentTaskUtils.waitForEvent(iframe, "load");

        await content.SpecialPowers.spawn(
          iframe,
          [iframeORIGIN, key, value],
          async function(ORIGIN, key, value) {
            is(
              content.window.origin,
              ORIGIN,
              `Navigate to ${ORIGIN} as expected`
            );

            let value1 = content.window.sessionStorage.getItem(key);
            is(
              value1,
              value,
              `SessionStorage for ${key} in ${content.window.origin} is preserved`
            );
          }
        );
      }
    );
  });
});
