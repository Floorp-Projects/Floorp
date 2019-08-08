/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

AddonTestUtils.initMochitest(this);

// Base64-encoded "Fake icon data".
const FAKE_ICON_DATA = "RmFrZSBpY29uIGRhdGE=";

// Base64-encoded "HTTP icon data".
const HTTP_ICON_DATA = "SFRUUCBpY29uIGRhdGE=";
const HTTP_ICON_URL = "http://example.org/ico.png";
const server = AddonTestUtils.createHttpServer({ hosts: ["example.org"] });
server.registerPathHandler("/ico.png", (request, response) => {
  response.write(atob(HTTP_ICON_DATA));
});

function promiseEngineIconLoaded(engineName) {
  return TestUtils.topicObserved(
    "browser-search-engine-modified",
    (engine, verb) => {
      engine.QueryInterface(Ci.nsISearchEngine);
      return (
        verb == "engine-changed" && engine.name == engineName && engine.iconURI
      );
    }
  );
}

add_task(async function test_search_favicon() {
  let searchExt = ExtensionTestUtils.loadExtension({
    manifest: {
      icons: {
        16: "someFavicon.png",
      },
      chrome_settings_overrides: {
        search_provider: {
          name: "Engine Only",
          search_url: "https://example.com/",
        },
      },
    },
    files: {
      "someFavicon.png": atob(FAKE_ICON_DATA),
    },
    useAddonManager: "temporary",
  });

  let searchExtWithBadIcon = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_settings_overrides: {
        search_provider: {
          name: "Bad Icon",
          search_url: "https://example.net/",
          favicon_url: "moz-extension://invalid/iDoNotExist.png",
        },
      },
    },
    useAddonManager: "temporary",
  });

  let searchExtWithHttpIcon = ExtensionTestUtils.loadExtension({
    manifest: {
      chrome_settings_overrides: {
        search_provider: {
          name: "HTTP Icon",
          search_url: "https://example.org/",
          favicon_url: HTTP_ICON_URL,
        },
      },
    },
    useAddonManager: "temporary",
  });

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["search"],
      icons: {
        16: "myFavicon.png",
      },
      chrome_settings_overrides: {
        search_provider: {
          name: "My Engine",
          search_url: "https://example.org/",
        },
      },
    },
    files: {
      "myFavicon.png": imageBuffer,
    },
    useAddonManager: "temporary",
    async background() {
      let engines = await browser.search.get();
      browser.test.sendMessage("engines", {
        badEngine: engines.find(engine => engine.name === "Bad Icon"),
        httpEngine: engines.find(engine => engine.name === "HTTP Icon"),
        myEngine: engines.find(engine => engine.name === "My Engine"),
        otherEngine: engines.find(engine => engine.name === "Engine Only"),
      });
    },
  });

  await searchExt.startup();
  await AddonTestUtils.waitForSearchProviderStartup(searchExt);

  await searchExtWithBadIcon.startup();
  await AddonTestUtils.waitForSearchProviderStartup(searchExtWithBadIcon);

  // TODO bug 1571718: browser.search.get should behave correctly (i.e return
  // the icon) even if the icon did not finish loading when the API was called.
  // Currently calling it too early returns undefined, so just wait until the
  // icon has loaded before calling browser.search.get.
  let httpIconLoaded = promiseEngineIconLoaded("HTTP Icon");
  await searchExtWithHttpIcon.startup();
  await AddonTestUtils.waitForSearchProviderStartup(searchExtWithHttpIcon);
  await httpIconLoaded;

  await extension.startup();
  await AddonTestUtils.waitForSearchProviderStartup(extension);

  let engines = await extension.awaitMessage("engines");

  // An extension's own icon can surely be accessed by the extension, so its
  // favIconUrl can be the moz-extension:-URL itself.
  Assert.deepEqual(
    engines.myEngine,
    {
      name: "My Engine",
      isDefault: false,
      alias: undefined,
      favIconUrl: `moz-extension://${extension.uuid}/myFavicon.png`,
    },
    "browser.search.get result for own extension"
  );

  // favIconUrl of other engines need to be in base64-encoded form.
  Assert.deepEqual(
    engines.otherEngine,
    {
      name: "Engine Only",
      isDefault: false,
      alias: undefined,
      favIconUrl: `data:image/png;base64,${FAKE_ICON_DATA}`,
    },
    "browser.search.get result for other extension"
  );

  // HTTP URLs should be provided as-is.
  Assert.deepEqual(
    engines.httpEngine,
    {
      name: "HTTP Icon",
      isDefault: false,
      alias: undefined,
      favIconUrl: `data:image/png;base64,${HTTP_ICON_DATA}`,
    },
    "browser.search.get result for extension with HTTP icon URL"
  );

  // When the favicon does not exists, the favIconUrl must be unset.
  Assert.deepEqual(
    engines.badEngine,
    {
      name: "Bad Icon",
      isDefault: false,
      alias: undefined,
      favIconUrl: undefined,
    },
    "browser.search.get result for other extension with non-existing icon"
  );

  await extension.unload();
  await searchExt.unload();
  await searchExtWithBadIcon.unload();
  await searchExtWithHttpIcon.unload();
});
