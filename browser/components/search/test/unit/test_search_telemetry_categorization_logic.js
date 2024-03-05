/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test ensures we are correctly applying the SERP categorization logic to
 * the domains that have been extracted from the SERP.
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  SearchSERPCategorization: "resource:///modules/SearchSERPTelemetry.sys.mjs",
  SearchSERPDomainToCategoriesMap:
    "resource:///modules/SearchSERPTelemetry.sys.mjs",
  SearchSERPTelemetryUtils: "resource:///modules/SearchSERPTelemetry.sys.mjs",
});

ChromeUtils.defineLazyGetter(this, "gCryptoHash", () => {
  return Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
});

function convertDomainsToHashes(domainsToCategories) {
  let newObj = {};
  for (let [key, value] of Object.entries(domainsToCategories)) {
    gCryptoHash.init(gCryptoHash.SHA256);
    let bytes = new TextEncoder().encode(key);
    gCryptoHash.update(bytes, key.length);
    let hash = gCryptoHash.finish(true);
    newObj[hash] = value;
  }
  return newObj;
}

const TEST_DOMAIN_TO_CATEGORIES_MAP_SIMPLE = convertDomainsToHashes({
  "test1.com": [2, 90],
  "test2.com": [2, 95],
  "test3.com": [2, 78, 4, 10],
  "test4.com": [2, 56, 4, 24],
  "test5.com": [2, 89],
  "test6.com": [2, 43],
  "test7.com": [2, 65],
  "test8.com": [2, 67],
  "test9.com": [2, 89],
  "test10.com": [2, 99],
});

const TEST_DOMAIN_TO_CATEGORIES_MAP_INCONCLUSIVE = convertDomainsToHashes({
  "test11.com": [0, 0],
  "test12.com": [0, 0],
  "test13.com": [0, 0],
  "test14.com": [0, 0],
  "test15.com": [0, 0],
  "test16.com": [0, 0],
  "test17.com": [0, 0],
  "test18.com": [0, 0],
  "test19.com": [0, 0],
  "test20.com": [0, 0],
});

const TEST_DOMAIN_TO_CATEGORIES_MAP_UNKNOWN_AND_INCONCLUSIVE =
  convertDomainsToHashes({
    "test31.com": [0, 0],
    "test32.com": [0, 0],
    "test33.com": [0, 0],
    "test34.com": [0, 0],
    "test35.com": [0, 0],
  });

const TEST_DOMAIN_TO_CATEGORIES_MAP_ALL_TYPES = convertDomainsToHashes({
  "test51.com": [3, 90],
  "test52.com": [3, 88],
  "test53.com": [3, 90, 6, 2],
  "test54.com": [3, 78, 6, 7],
  "test55.com": [3, 97],
  "test56.com": [0, 0],
  "test57.com": [0, 0],
});

const TEST_DOMAIN_TO_CATEGORIES_MAP_TIE = convertDomainsToHashes({
  "test41.com": [1, 50, 2, 50],
  "test42.com": [1, 50, 2, 50],
  "test43.com": [1, 50, 2, 50],
  "test44.com": [1, 50, 2, 50],
  "test45.com": [1, 50, 2, 50],
  "test46.com": [3, 50, 4, 50],
  "test47.com": [5, 50, 6, 50],
  "test48.com": [7, 50, 8, 50],
  "test49.com": [9, 50, 10, 50],
  "test50.com": [11, 50, 12, 50],
});

const TEST_DOMAIN_TO_CATEGORIES_MAP_RANK_PENALIZATION_1 =
  convertDomainsToHashes({
    "test51.com": [1, 45],
    "test52.com": [2, 45],
    "test53.com": [3, 45],
    "test54.com": [4, 45],
    "test55.com": [5, 45],
    "test56.com": [6, 45],
    "test57.com": [7, 45],
    "test58.com": [8, 45],
    "test59.com": [9, 45],
    "test60.com": [10, 45],
  });

const TEST_DOMAIN_TO_CATEGORIES_MAP_RANK_PENALIZATION_2 =
  convertDomainsToHashes({
    "test61.com": [1, 35, 2, 4],
    "test62.com": [1, 5, 2, 94],
  });

add_setup(async () => {
  do_get_profile();
  Services.prefs.setBoolPref(
    "browser.search.serpEventTelemetryCategorization.enabled",
    true
  );
  await SearchSERPDomainToCategoriesMap.init();
});

add_task(async function test_categorization_simple() {
  await SearchSERPDomainToCategoriesMap.overrideMapForTests(
    TEST_DOMAIN_TO_CATEGORIES_MAP_SIMPLE
  );

  let domains = new Set([
    "test1.com",
    "test2.com",
    "test3.com",
    "test4.com",
    "test5.com",
    "test6.com",
    "test7.com",
    "test8.com",
    "test9.com",
    "test10.com",
  ]);

  let resultsToReport = await SearchSERPCategorization.applyCategorizationLogic(
    domains
  );

  Assert.deepEqual(
    resultsToReport,
    { category: "2", num_domains: 10, num_inconclusive: 0, num_unknown: 0 },
    "Should report the correct values for categorizing the SERP."
  );
});

add_task(async function test_categorization_inconclusive() {
  await SearchSERPDomainToCategoriesMap.overrideMapForTests(
    TEST_DOMAIN_TO_CATEGORIES_MAP_INCONCLUSIVE
  );

  let domains = new Set([
    "test11.com",
    "test12.com",
    "test13.com",
    "test14.com",
    "test15.com",
    "test16.com",
    "test17.com",
    "test18.com",
    "test19.com",
    "test20.com",
  ]);

  let resultsToReport = await SearchSERPCategorization.applyCategorizationLogic(
    domains
  );

  Assert.deepEqual(
    resultsToReport,
    {
      category: SearchSERPTelemetryUtils.CATEGORIZATION.INCONCLUSIVE,
      num_domains: 10,
      num_inconclusive: 10,
      num_unknown: 0,
    },
    "Should report the correct values for categorizing the SERP."
  );
});

add_task(async function test_categorization_unknown() {
  // Reusing TEST_DOMAIN_TO_CATEGORIES_MAP_SIMPLE since none of this task's
  // domains will be keys within it.
  await SearchSERPDomainToCategoriesMap.overrideMapForTests(
    TEST_DOMAIN_TO_CATEGORIES_MAP_SIMPLE
  );

  let domains = new Set([
    "test21.com",
    "test22.com",
    "test23.com",
    "test24.com",
    "test25.com",
    "test26.com",
    "test27.com",
    "test28.com",
    "test29.com",
    "test30.com",
  ]);

  let resultsToReport = await SearchSERPCategorization.applyCategorizationLogic(
    domains
  );

  Assert.deepEqual(
    resultsToReport,
    {
      category: SearchSERPTelemetryUtils.CATEGORIZATION.INCONCLUSIVE,
      num_domains: 10,
      num_inconclusive: 0,
      num_unknown: 10,
    },
    "Should report the correct values for categorizing the SERP."
  );
});

add_task(async function test_categorization_unknown_and_inconclusive() {
  await SearchSERPDomainToCategoriesMap.overrideMapForTests(
    TEST_DOMAIN_TO_CATEGORIES_MAP_UNKNOWN_AND_INCONCLUSIVE
  );

  let domains = new Set([
    "test31.com",
    "test32.com",
    "test33.com",
    "test34.com",
    "test35.com",
    "test36.com",
    "test37.com",
    "test38.com",
    "test39.com",
    "test40.com",
  ]);

  let resultsToReport = await SearchSERPCategorization.applyCategorizationLogic(
    domains
  );

  Assert.deepEqual(
    resultsToReport,
    {
      category: SearchSERPTelemetryUtils.CATEGORIZATION.INCONCLUSIVE,
      num_domains: 10,
      num_inconclusive: 5,
      num_unknown: 5,
    },
    "Should report the correct values for categorizing the SERP."
  );
});

// Tests a mixture of categorized, inconclusive and unknown domains.
add_task(async function test_categorization_all_types() {
  await SearchSERPDomainToCategoriesMap.overrideMapForTests(
    TEST_DOMAIN_TO_CATEGORIES_MAP_ALL_TYPES
  );

  // First 5 domains are categorized, 6th and 7th are inconclusive and the last
  // 3 are unknown.
  let domains = new Set([
    "test51.com",
    "test52.com",
    "test53.com",
    "test54.com",
    "test55.com",
    "test56.com",
    "test57.com",
    "test58.com",
    "test59.com",
    "test60.com",
  ]);

  let resultsToReport = await SearchSERPCategorization.applyCategorizationLogic(
    domains
  );

  Assert.deepEqual(
    resultsToReport,
    {
      category: "3",
      num_domains: 10,
      num_inconclusive: 2,
      num_unknown: 3,
    },
    "Should report the correct values for categorizing the SERP."
  );
});

add_task(async function test_categorization_tie() {
  await SearchSERPDomainToCategoriesMap.overrideMapForTests(
    TEST_DOMAIN_TO_CATEGORIES_MAP_TIE
  );

  let domains = new Set([
    "test41.com",
    "test42.com",
    "test43.com",
    "test44.com",
    "test45.com",
    "test46.com",
    "test47.com",
    "test48.com",
    "test49.com",
    "test50.com",
  ]);

  let resultsToReport = await SearchSERPCategorization.applyCategorizationLogic(
    domains
  );

  Assert.equal(
    [1, 2].includes(resultsToReport.category),
    true,
    "Category should be one of the 2 categories with the max score."
  );
  delete resultsToReport.category;
  Assert.deepEqual(
    resultsToReport,
    {
      num_domains: 10,
      num_inconclusive: 0,
      num_unknown: 0,
    },
    "Should report the correct counts for the various domain types."
  );
});

add_task(async function test_rank_penalization_equal_scores() {
  await SearchSERPDomainToCategoriesMap.overrideMapForTests(
    TEST_DOMAIN_TO_CATEGORIES_MAP_RANK_PENALIZATION_1
  );

  let domains = new Set([
    "test51.com",
    "test52.com",
    "test53.com",
    "test54.com",
    "test55.com",
    "test56.com",
    "test57.com",
    "test58.com",
    "test59.com",
    "test60.com",
  ]);

  let resultsToReport = await SearchSERPCategorization.applyCategorizationLogic(
    domains
  );

  Assert.deepEqual(
    resultsToReport,
    { category: "1", num_domains: 10, num_inconclusive: 0, num_unknown: 0 },
    "Should report the correct values for categorizing the SERP."
  );
});

add_task(async function test_rank_penalization_highest_score_lower_on_page() {
  await SearchSERPDomainToCategoriesMap.overrideMapForTests(
    TEST_DOMAIN_TO_CATEGORIES_MAP_RANK_PENALIZATION_2
  );

  let domains = new Set(["test61.com", "test62.com"]);

  let resultsToReport = await SearchSERPCategorization.applyCategorizationLogic(
    domains
  );

  Assert.deepEqual(
    resultsToReport,
    { category: "2", num_domains: 2, num_inconclusive: 0, num_unknown: 0 },
    "Should report the correct values for categorizing the SERP."
  );
});
