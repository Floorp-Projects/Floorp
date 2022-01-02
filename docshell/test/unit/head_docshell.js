/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonTestUtils: "resource://testing-common/AddonTestUtils.jsm",
  NetUtil: "resource://gre/modules/NetUtil.jsm",
  SearchUtils: "resource://gre/modules/SearchUtils.jsm",
  SearchTestUtils: "resource://testing-common/SearchTestUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  TestUtils: "resource://testing-common/TestUtils.jsm",
});

var dirSvc = Services.dirSvc;
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

  Services.prefs.setBoolPref("browser.search.modernConfig", true);
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

async function addTestEngines() {
  // This is a hack, ideally we should be setting up a configuration with
  // built-in engines, but the `chrome_settings_overrides` section that
  // WebExtensions need is only defined for browser/
  await Services.search.addPolicyEngine({
    description: "urifixup search engine",
    chrome_settings_overrides: {
      search_provider: {
        name: kSearchEngineID,
        search_url: kSearchEngineURL,
      },
    },
  });
  await Services.search.addPolicyEngine({
    description: "urifixup private search engine",
    chrome_settings_overrides: {
      search_provider: {
        name: kPrivateSearchEngineID,
        search_url: kPrivateSearchEngineURL,
      },
    },
  });
  await Services.search.addPolicyEngine({
    description: "urifixup POST search engine",
    chrome_settings_overrides: {
      search_provider: {
        name: kPostSearchEngineID,
        search_url: kPostSearchEngineURL,
        search_url_post_params: kPostSearchEngineData,
      },
    },
  });
}
