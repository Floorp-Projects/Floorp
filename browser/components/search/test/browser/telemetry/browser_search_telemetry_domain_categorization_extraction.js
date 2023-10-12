/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/*
 * This test ensures we are correctly extracting domains from a SERP.
 */

ChromeUtils.defineESModuleGetters(this, {
  SearchSERPCategorization: "resource:///modules/SearchSERPTelemetry.sys.mjs",
  SearchUtils: "resource://gre/modules/SearchUtils.sys.mjs",
});

const TESTS = [
  {
    title: "Extract domain from href (absolute URL) - one link.",
    extractorInfos: [
      {
        selectors:
          '#test1 [data-layout="organic"] a[data-testid="result-title-a"]',
        method: "href",
      },
    ],
    expectedDomains: ["foobar.com"],
  },
  {
    title: "Extract domain from href (absolute URL) - multiple links.",
    extractorInfos: [
      {
        selectors:
          '#test2 [data-layout="organic"] a[data-testid="result-title-a"]',
        method: "href",
      },
    ],
    expectedDomains: ["foo.com", "bar.com", "baz.com", "qux.com"],
  },
  {
    title: "Extract domain from href (relative URL).",
    extractorInfos: [
      {
        selectors:
          '#test3 [data-layout="organic"] a[data-testid="result-title-a"]',
        method: "href",
      },
    ],
    expectedDomains: ["example.org"],
  },
  {
    title: "Extract domain from data attribute - one link.",
    extractorInfos: [
      {
        selectors: "#test4 [data-dtld]",
        method: "data-attribute",
        options: {
          dataAttributeKey: "dtld",
        },
      },
    ],
    expectedDomains: ["www.abc.com"],
  },
  {
    title: "Extract domain from data attribute - multiple links.",
    extractorInfos: [
      {
        selectors: "#test5 [data-dtld]",
        method: "data-attribute",
        options: {
          dataAttributeKey: "dtld",
        },
      },
    ],
    expectedDomains: [
      "www.foo.com",
      "www.bar.com",
      "www.baz.com",
      "www.qux.com",
    ],
  },
  {
    title: "Extract domain from an href's query param value.",
    extractorInfos: [
      {
        selectors:
          '#test6 .js-carousel-item-title, #test6 [data-layout="ad"] [data-testid="result-title-a"]',
        method: "href",
        options: {
          queryParamKey: "ad_domain",
        },
      },
    ],
    expectedDomains: ["def.com"],
  },
  {
    title: "Extraction preserves order of domains within the page.",
    extractorInfos: [
      {
        selectors:
          '#test7 [data-layout="organic"] a[data-testid="result-title-a"]',
        method: "href",
      },
      {
        selectors: "#test7 [data-dtld]",
        method: "data-attribute",
        options: {
          dataAttributeKey: "dtld",
        },
      },
      {
        selectors:
          '#test7 .js-carousel-item-title, #test7 [data-layout="ad"] [data-testid="result-title-a"]',
        method: "href",
        options: {
          queryParamKey: "ad_domain",
        },
      },
    ],
    expectedDomains: ["foobar.com", "www.abc.com", "def.com"],
  },
  {
    title: "No elements match the selectors.",
    extractorInfos: [
      {
        selectors:
          '#test8 [data-layout="organic"] a[data-testid="result-title-a"]',
        method: "href",
      },
    ],
    expectedDomains: [],
  },
  {
    title: "Data attribute is present, but value is missing.",
    extractorInfos: [
      {
        selectors: "#test9 [data-dtld]",
        method: "data-attribute",
        options: {
          dataAttributeKey: "dtld",
        },
      },
    ],
    expectedDomains: [],
  },
  {
    title: "Query param is present, but value is missing.",
    extractorInfos: [
      {
        selectors: '#test10 [data-layout="ad"] [data-testid="result-title-a"]',
        method: "href",
        options: {
          queryParamKey: "ad_domain",
        },
      },
    ],
    expectedDomains: [],
  },
  {
    title: "Non-standard URL scheme.",
    extractorInfos: [
      {
        selectors:
          '#test11 [data-layout="organic"] a[data-testid="result-title-a"]',
        method: "href",
      },
    ],
    expectedDomains: [],
  },
];

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.log", true],
      ["browser.search.serpEventTelemetry.enabled", true],
      ["browser.search.serpEventTelemetryCategorization.enabled", true],
    ],
  });

  await SearchSERPTelemetry.init();

  registerCleanupFunction(async () => {
    resetTelemetry();
  });
});

add_task(async function test_domain_extraction_heuristics() {
  resetTelemetry();
  let url = getSERPUrl("searchTelemetryDomainExtraction.html");
  info(
    "Load a sample SERP where domains need to be extracted in different ways."
  );
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  for (let currentTest of TESTS) {
    if (currentTest.title) {
      info(currentTest.title);
    }
    let expectedDomains = new Set(currentTest.expectedDomains);
    let actualDomains = await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [currentTest.extractorInfos],
      extractorInfos => {
        const { domainExtractor } = ChromeUtils.importESModule(
          "resource:///actors/SearchSERPTelemetryChild.sys.mjs"
        );
        return domainExtractor.extractDomainsFromDocument(
          content.document,
          extractorInfos
        );
      }
    );

    Assert.deepEqual(
      Array.from(actualDomains),
      Array.from(expectedDomains),
      "Domains should have been extracted correctly."
    );
  }

  BrowserTestUtils.removeTab(tab);
});
