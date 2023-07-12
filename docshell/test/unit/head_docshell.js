/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  AddonTestUtils: "resource://testing-common/AddonTestUtils.sys.mjs",
  HttpServer: "resource://testing-common/httpd.sys.mjs",
  NetUtil: "resource://gre/modules/NetUtil.sys.mjs",
  SearchTestUtils: "resource://testing-common/SearchTestUtils.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
  TestUtils: "resource://testing-common/TestUtils.sys.mjs",
});

var profileDir = do_get_profile();

const kSearchEngineID = "test_urifixup_search_engine";
const kSearchEngineURL = "https://www.example.org/?search={searchTerms}";
const kPrivateSearchEngineID = "test_urifixup_search_engine_private";
const kPrivateSearchEngineURL =
  "https://www.example.org/?private={searchTerms}";
const kPostSearchEngineID = "test_urifixup_search_engine_post";
const kPostSearchEngineURL = "https://www.example.org/";
const kPostSearchEngineData = "q={searchTerms}";

const SEARCH_CONFIG = [
  {
    appliesTo: [
      {
        included: {
          everywhere: true,
        },
      },
    ],
    default: "yes",
    webExtension: {
      id: "fixup_search@search.mozilla.org",
    },
  },
];

async function setupSearchService() {
  SearchTestUtils.init(this);

  AddonTestUtils.init(this);
  AddonTestUtils.overrideCertDB();
  AddonTestUtils.createAppInfo(
    "xpcshell@tests.mozilla.org",
    "XPCShell",
    "1",
    "42"
  );

  await SearchTestUtils.useTestEngines(".", null, SEARCH_CONFIG);
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
}

/**
 * After useHttpServer() is called, this string contains the URL of the "data"
 * directory, including the final slash.
 */
var gDataUrl;

/**
 * Initializes the HTTP server and ensures that it is terminated when tests end.
 *
 * @param {string} dir
 *   The test sub-directory to use for the engines.
 * @returns {HttpServer}
 *   The HttpServer object in case further customization is needed.
 */
function useHttpServer(dir = "data") {
  let httpServer = new HttpServer();
  httpServer.start(-1);
  httpServer.registerDirectory("/", do_get_cwd());
  gDataUrl = `http://localhost:${httpServer.identity.primaryPort}/${dir}/`;
  registerCleanupFunction(async function cleanup_httpServer() {
    await new Promise(resolve => {
      httpServer.stop(resolve);
    });
  });
  return httpServer;
}

async function addTestEngines() {
  useHttpServer();
  // This is a hack, ideally we should be setting up a configuration with
  // built-in engines, but the `chrome_settings_overrides` section that
  // WebExtensions need is only defined for browser/
  await SearchTestUtils.promiseNewSearchEngine({
    url: `${gDataUrl}/engine.xml`,
  });
  await SearchTestUtils.promiseNewSearchEngine({
    url: `${gDataUrl}/enginePrivate.xml`,
  });
  await SearchTestUtils.promiseNewSearchEngine({
    url: `${gDataUrl}/enginePost.xml`,
  });
}
