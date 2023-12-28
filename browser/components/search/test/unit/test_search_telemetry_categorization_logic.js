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

const TEST_DOMAIN_TO_CATEGORIES_MAP_SIMPLE = {
  "byVQ4ej7T7s2xf/cPqgMyw==": [2, 90],
  "1TEnSjgNCuobI6olZinMiQ==": [2, 95],
  "/Bnju09b9iBPjg7K+5ENIw==": [2, 78, 4, 10],
  "Ja6RJq5LQftdl7NQrX1avQ==": [2, 56, 4, 24],
  "Jy26Qt99JrUderAcURtQ5A==": [2, 89],
  "sZnJyyzY9QcN810Q6jfbvw==": [2, 43],
  "QhmteGKeYk0okuB/bXzwRw==": [2, 65],
  "CKQZZ1IJjzjjE4LUV8vUSg==": [2, 67],
  "FK7mL5E1JaE6VzOiGMmlZg==": [2, 89],
  "mzcR/nhDcrs0ed4kTf+ZFg==": [2, 99],
};

const TEST_DOMAIN_TO_CATEGORIES_MAP_INCONCLUSIVE = {
  "IkOfhoSlHTMIZzWXkYf7fg==": [0, 0],
  "PIAHxeaBOeDNY2tvZKqQuw==": [0, 0],
  "DKx2mqmFtEvxrHAqpwSevA==": [0, 0],
  "DlZKnz9ryYqbxJq9wodzlA==": [0, 0],
  "n3NWT4N9JlKX0I7MUtAsYg==": [0, 0],
  "A6KyupOlu5zXt8loti90qw==": [0, 0],
  "gf5rpseruOaq8nXOSJPG3Q==": [0, 0],
  "vlQYOvbcbAp6sMx54OwqCQ==": [0, 0],
  "8PcaPATLgmHD9SR0/961Sw==": [0, 0],
  "l+hLycEAW2v/OPE/XFpNwQ==": [0, 0],
};

const TEST_DOMAIN_TO_CATEGORIES_MAP_UNKNOWN_AND_INCONCLUSIVE = {
  "CEA642T3hV+Fdi2PaRH9BQ==": [0, 0],
  "cVqopYLASYxcWdDW4F+w2w==": [0, 0],
  "X61OdTU20n8pxZ76K2eAHg==": [0, 0],
  "/srrOggOAwgaBGCsPdC4bA==": [0, 0],
  "onnMGn+MmaCQx3RNLBzGOQ==": [0, 0],
};

const TEST_DOMAIN_TO_CATEGORIES_MAP_ALL_TYPES = {
  "VSXaqgDKYWrJ/yjsFomUdg==": [3, 90],
  "6re74Kk34n2V6VCdLmCD5w==": [3, 88],
  "s8gOGIaFnly5hHX7nPncnw==": [3, 90, 6, 2],
  "zfRJyKV+2jd1RKNsSHm9pw==": [3, 78, 6, 7],
  "zcW+KbRfLRO6Dljf5qnuwQ==": [3, 97],
  "Rau9mfbBcIRiRQIliUxkow==": [0, 0],
  "4AFhUOmLQ8804doOsI4jBA==": [0, 0],
};

const TEST_DOMAIN_TO_CATEGORIES_MAP_TIE = {
  "fmEqRSc+pBr9noi0l99nGw==": [1, 50, 2, 50],
  "cms8ipz0JQ3WS9o48RtvnQ==": [1, 50, 2, 50],
  "y8Haj7Qdmx+k762RaxCPvA==": [1, 50, 2, 50],
  "tCbLmi5xJ/OrF8tbRm8PrA==": [1, 50, 2, 50],
  "uYNQECmDShqI409HrSTdLQ==": [1, 50, 2, 50],
  "D88hdsmzLWIXYhkrDal33w==": [3, 50, 4, 50],
  "1mhx0I0B4cEaI91x8zor7Q==": [5, 50, 6, 50],
  "dVZYATQixuBHmalCFR9+Lw==": [7, 50, 8, 50],
  "pdOFJG49D7hE/+FtsWDihQ==": [9, 50, 10, 50],
  "+gl+dBhWE0nx0AM69m2g5w==": [11, 50, 12, 50],
};

const TEST_DOMAIN_TO_CATEGORIES_MAP_RANK_PENALIZATION_1 = {
  "VSXaqgDKYWrJ/yjsFomUdg==": [1, 45],
  "6re74Kk34n2V6VCdLmCD5w==": [2, 45],
  "s8gOGIaFnly5hHX7nPncnw==": [3, 45],
  "zfRJyKV+2jd1RKNsSHm9pw==": [4, 45],
  "zcW+KbRfLRO6Dljf5qnuwQ==": [5, 45],
  "Rau9mfbBcIRiRQIliUxkow==": [6, 45],
  "4AFhUOmLQ8804doOsI4jBA==": [7, 45],
  "YZ3aEL73MR+Cjog0D7A24w==": [8, 45],
  "crMclD9rwInEQ30DpZLg+g==": [9, 45],
  "/r7oPRoE6LJAE95nuwmu7w==": [10, 45],
};

const TEST_DOMAIN_TO_CATEGORIES_MAP_RANK_PENALIZATION_2 = {
  "sHWSmFwSYL3snycBZCY8Kg==": [1, 35, 2, 4],
  "FZ5zPYh6ByI0KGWKkmpDoA==": [1, 5, 2, 94],
};

const TEST_DOMAIN_TO_CATEGORIES_MAP_RANK_PENALIZATION_3 = {
  "WvodmXTKbmLPVwFSai5uMQ==": [0, 52, 3, 45],
};

add_setup(async () => {
  Services.prefs.setBoolPref(
    "browser.search.serpEventTelemetryCategorization.enabled",
    true
  );
});

add_task(async function test_categorization_simple() {
  SearchSERPDomainToCategoriesMap.overrideMapForTests(
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

  let resultsToReport =
    SearchSERPCategorization.applyCategorizationLogic(domains);

  Assert.deepEqual(
    resultsToReport,
    { category: "2", num_domains: 10, num_inconclusive: 0, num_unknown: 0 },
    "Should report the correct values for categorizing the SERP."
  );
});

add_task(async function test_categorization_inconclusive() {
  SearchSERPDomainToCategoriesMap.overrideMapForTests(
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

  let resultsToReport =
    SearchSERPCategorization.applyCategorizationLogic(domains);

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
  SearchSERPDomainToCategoriesMap.overrideMapForTests(
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

  let resultsToReport =
    SearchSERPCategorization.applyCategorizationLogic(domains);

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
  SearchSERPDomainToCategoriesMap.overrideMapForTests(
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

  let resultsToReport =
    SearchSERPCategorization.applyCategorizationLogic(domains);

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
  SearchSERPDomainToCategoriesMap.overrideMapForTests(
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

  let resultsToReport =
    SearchSERPCategorization.applyCategorizationLogic(domains);

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
  SearchSERPDomainToCategoriesMap.overrideMapForTests(
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

  let resultsToReport =
    SearchSERPCategorization.applyCategorizationLogic(domains);

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
  SearchSERPDomainToCategoriesMap.overrideMapForTests(
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

  let resultsToReport =
    SearchSERPCategorization.applyCategorizationLogic(domains);

  Assert.deepEqual(
    resultsToReport,
    { category: "1", num_domains: 10, num_inconclusive: 0, num_unknown: 0 },
    "Should report the correct values for categorizing the SERP."
  );
});

add_task(async function test_rank_penalization_highest_score_lower_on_page() {
  SearchSERPDomainToCategoriesMap.overrideMapForTests(
    TEST_DOMAIN_TO_CATEGORIES_MAP_RANK_PENALIZATION_2
  );

  let domains = new Set(["test61.com", "test62.com"]);

  let resultsToReport =
    SearchSERPCategorization.applyCategorizationLogic(domains);

  Assert.deepEqual(
    resultsToReport,
    { category: "2", num_domains: 2, num_inconclusive: 0, num_unknown: 0 },
    "Should report the correct values for categorizing the SERP."
  );
});

add_task(async function test_high_inconclusive_causes_domain_to_be_ignored() {
  SearchSERPDomainToCategoriesMap.overrideMapForTests(
    TEST_DOMAIN_TO_CATEGORIES_MAP_RANK_PENALIZATION_3
  );

  let domains = new Set(["test63.com"]);

  let resultsToReport =
    SearchSERPCategorization.applyCategorizationLogic(domains);

  Assert.deepEqual(
    resultsToReport,
    {
      category: SearchSERPTelemetryUtils.CATEGORIZATION.INCONCLUSIVE,
      num_domains: 1,
      num_inconclusive: 1,
      num_unknown: 0,
    },
    "Should report the correct values for categorizing the SERP."
  );
});
