"use strict";

const DIRPATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  ""
);
const PATH = DIRPATH + "file_empty.html";

const ORIGIN1 = "https://example.com";
const ORIGIN2 = "https://example.org";
const URL1 = `${ORIGIN1}/${PATH}`;
const URL2 = `${ORIGIN2}/${PATH}`;
const URL1_WITH_COOP_COEP = `${ORIGIN1}/${DIRPATH}file_coop_coep.html`;

add_task(async function () {
  await BrowserTestUtils.withNewTab(URL1, async function (browser) {
    const key = "key";
    const value = "value";

    info(
      `Verifying sessionStorage is preserved after navigating to a ` +
        `cross-origin site and then navigating back`
    );

    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "privacy.partition.always_partition_third_party_non_cookie_storage",
          false,
        ],
      ],
    });

    BrowserTestUtils.startLoadingURIString(browser, URL1);
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
          `SessionStorage for ${key} in ${content.window.origin} is null ` +
            `since it's the first visit`
        );

        content.window.sessionStorage.setItem(key, value);

        let value2 = content.window.sessionStorage.getItem(key);
        is(
          value2,
          value,
          `SessionStorage for ${key} in ${content.window.origin} is set ` +
            `correctly`
        );
      }
    );

    BrowserTestUtils.startLoadingURIString(browser, URL2);
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
          `SessionStorage for ${key} in ${content.window.origin} is null ` +
            `since it's the first visit`
        );
      }
    );

    BrowserTestUtils.startLoadingURIString(browser, URL1);
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

    BrowserTestUtils.startLoadingURIString(browser, URL2);
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
          async function (ORIGIN, key, value) {
            is(
              content.window.origin,
              ORIGIN,
              `Navigate to ${ORIGIN} as expected`
            );

            // Bug 1746646: Make mochitests work with TCP enabled (cookieBehavior = 5)
            // Acquire storage access permission here so that the iframe has
            // first-party access to the sessionStorage. Without this, it is
            // isolated and this test will always fail
            SpecialPowers.wrap(content.document).notifyUserGestureActivation();
            await SpecialPowers.addPermission(
              "storageAccessAPI",
              true,
              content.window.location.href
            );
            await SpecialPowers.wrap(content.document).requestStorageAccess();

            let value1 = content.window.sessionStorage.getItem(key);
            is(
              value1,
              value,
              `SessionStorage for ${key} in ${content.window.origin} is ` +
                `preserved`
            );
          }
        );
      }
    );

    info(`Verifying SSCache is loaded to the content process only once`);

    BrowserTestUtils.startLoadingURIString(browser, URL1);
    await BrowserTestUtils.browserLoaded(browser);

    await SpecialPowers.spawn(
      browser,
      [ORIGIN1, URL1, key, value],
      async (ORIGIN, iframeURL, key, value) => {
        is(content.window.origin, ORIGIN, `Navigate to ${ORIGIN} as expected`);

        let iframe = content.document.createElement("iframe");
        iframe.src = iframeURL;
        content.document.body.appendChild(iframe);
        await ContentTaskUtils.waitForEvent(iframe, "load");

        await content.SpecialPowers.spawn(
          iframe,
          [ORIGIN, key, value],
          async function (ORIGIN, key, value) {
            is(
              content.window.origin,
              ORIGIN,
              `Load an iframe to ${ORIGIN} as expected`
            );

            let value1 = content.window.sessionStorage.getItem(key);
            is(
              value1,
              value,
              `SessionStorage for ${key} in ${content.window.origin} is ` +
                `preserved.`
            );

            // When we are here, it means we didn't hit the assertion for
            // ensuring a SSCache can only be loaded on the content process
            // once.
          }
        );
      }
    );

    info(
      `Verifying the sessionStorage for a tab shares between ` +
        `cross-origin-isolated and non cross-origin-isolated environments`
    );
    const anotherKey = `anotherKey`;
    const anotherValue = `anotherValue;`;

    BrowserTestUtils.startLoadingURIString(browser, URL1_WITH_COOP_COEP);
    await BrowserTestUtils.browserLoaded(browser);

    await SpecialPowers.spawn(
      browser,
      [ORIGIN1, key, value, anotherKey, anotherValue],
      async (ORIGIN, key, value, anotherKey, anotherValue) => {
        is(content.window.origin, ORIGIN, `Navigate to ${ORIGIN} as expected`);
        ok(
          content.window.crossOriginIsolated,
          `The window is cross-origin-isolated.`
        );

        let value1 = content.window.sessionStorage.getItem(key);
        is(
          value1,
          value,
          `SessionStorage for ${key} in ${content.window.origin} was ` +
            `propagated to COOP+COEP process correctly.`
        );

        let value2 = content.window.sessionStorage.getItem(anotherKey);
        is(
          value2,
          null,
          `SessionStorage for ${anotherKey} in ${content.window.origin} ` +
            `hasn't been set yet.`
        );

        content.window.sessionStorage.setItem(anotherKey, anotherValue);

        let value3 = content.window.sessionStorage.getItem(anotherKey);
        is(
          value3,
          anotherValue,
          `SessionStorage for ${anotherKey} in ${content.window.origin} ` +
            `was set as expected.`
        );
      }
    );

    BrowserTestUtils.startLoadingURIString(browser, URL1);
    await BrowserTestUtils.browserLoaded(browser);

    await SpecialPowers.spawn(
      browser,
      [ORIGIN1, key, value, anotherKey, anotherValue],
      async (ORIGIN, key, value, anotherKey, anotherValue) => {
        is(content.window.origin, ORIGIN, `Navigate to ${ORIGIN} as expected`);
        ok(
          !content.window.crossOriginIsolated,
          `The window is not cross-origin-isolated.`
        );

        let value1 = content.window.sessionStorage.getItem(key);
        is(
          value1,
          value,
          `SessionStorage for ${key} in ${content.window.origin} is ` +
            `preserved.`
        );

        let value2 = content.window.sessionStorage.getItem(anotherKey);
        is(
          value2,
          anotherValue,
          `SessionStorage for ${anotherKey} in ${content.window.origin} was ` +
            `propagated to non-COOP+COEP process correctly.`
        );
      }
    );
  });
});
