/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This test ensures we are correctly processing the domains that have been
 * extracted from a SERP.
 */

ChromeUtils.defineESModuleGetters(this, {
  BrowserSearchTelemetry: "resource:///modules/BrowserSearchTelemetry.sys.mjs",
  SearchSERPCategorization: "resource:///modules/SearchSERPTelemetry.sys.mjs",
  SearchSERPTelemetry: "resource:///modules/SearchSERPTelemetry.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
  sinon: "resource://testing-common/Sinon.sys.mjs",
});

// Links including the provider name are not extracted.
const PROVIDER = "example";

const TESTS = [
  {
    title: "Domains matching the provider.",
    domains: ["example.com", "www.example.com", "www.foobar.com"],
    expected: ["foobar.com"],
  },
  {
    title: "Second-level domains to a top-level domain.",
    domains: [
      "www.foobar.gc.ca",
      "www.foobar.gov.uk",
      "foobar.co.uk",
      "www.foobar.co.il",
    ],
    expected: ["foobar.gc.ca", "foobar.gov.uk", "foobar.co.uk", "foobar.co.il"],
  },
  {
    title: "Long subdomain.",
    domains: ["ab.cd.ef.gh.foobar.com"],
    expected: ["foobar.com"],
  },
  {
    title: "Same top-level domain.",
    domains: ["foobar.com", "www.foobar.com", "abc.def.foobar.com"],
    expected: ["foobar.com"],
  },
  {
    title: "Empty input.",
    domains: [""],
    expected: [],
  },
];

add_setup(async function () {
  Services.prefs.setBoolPref(SearchUtils.BROWSER_SEARCH_PREF + "log", true);
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "serpEventTelemetry.enabled",
    true
  );
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF +
      "serpEventTelemetryCategorization.enabled",
    true
  );

  // Required or else BrowserSearchTelemetry will throw.
  sinon.stub(BrowserSearchTelemetry, "shouldRecordSearchCount").returns(true);
  await SearchSERPTelemetry.init();
});

add_task(async function test_parsing_extracted_urls() {
  for (let i = 0; i < TESTS.length; i++) {
    let currentTest = TESTS[i];
    let domains = new Set(currentTest.domains);

    if (currentTest.title) {
      info(currentTest.title);
    }
    let expectedDomains = new Set(currentTest.expected);
    let actualDomains = SearchSERPCategorization.processDomains(
      domains,
      PROVIDER
    );

    Assert.deepEqual(
      Array.from(actualDomains),
      Array.from(expectedDomains),
      "Domains should have been parsed correctly."
    );
  }
});
