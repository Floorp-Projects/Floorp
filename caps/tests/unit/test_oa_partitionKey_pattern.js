/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests origin attributes partitionKey pattern matching.
 */

"use strict";

function testMatch(oa, pattern, shouldMatch = true) {
  let msg = `Origin attributes should ${
    shouldMatch ? "match" : "not match"
  } pattern.`;
  msg += ` oa: ${JSON.stringify(oa)} - pattern: ${JSON.stringify(pattern)}`;
  Assert.equal(
    ChromeUtils.originAttributesMatchPattern(oa, pattern),
    shouldMatch,
    msg
  );
}

function getPartitionKey(scheme, baseDomain, port) {
  if (!scheme || !baseDomain) {
    return "";
  }
  return `(${scheme},${baseDomain}${port ? `,${port}` : ``})`;
}

function getOAWithPartitionKey(scheme, baseDomain, port, oa = {}) {
  oa.partitionKey = getPartitionKey(scheme, baseDomain, port);
  return oa;
}

/**
 * Tests that an OriginAttributesPattern which is empty or only has an empty
 * partitionKeyPattern matches any partitionKey.
 */
add_task(async function test_empty_pattern_matches_any() {
  let list = [
    getOAWithPartitionKey("https", "example.com"),
    getOAWithPartitionKey("http", "example.net", 8080),
    getOAWithPartitionKey(),
  ];

  for (let oa of list) {
    testMatch(oa, {});
    testMatch(oa, { partitionKeyPattern: {} });
  }
});

/**
 * Tests that if a partitionKeyPattern is passed, but the partitionKey is
 * invalid, the pattern match will always fail.
 */
add_task(async function test_invalid_pk() {
  let list = [
    "()",
    "(,,)",
    "(https)",
    "(https,,)",
    "(example.com)",
    "(http,example.com,invalid)",
    "(http,example.com,8000,1000)",
  ].map(partitionKey => ({ partitionKey }));

  for (let oa of list) {
    testMatch(oa, {});
    testMatch(oa, { partitionKeyPattern: {} });
    testMatch(
      oa,
      { partitionKeyPattern: { baseDomain: "example.com" } },
      false
    );
    testMatch(oa, { partitionKeyPattern: { scheme: "https" } }, false);
  }
});

/**
 * Tests that if a pattern sets "partitionKey" it takes precedence over "partitionKeyPattern".
 */
add_task(async function test_string_overwrites_pattern() {
  let oa = getOAWithPartitionKey("https", "example.com", 8080, {
    userContextId: 2,
  });

  testMatch(oa, { partitionKey: oa.partitionKey });
  testMatch(oa, {
    partitionKey: oa.partitionKey,
    partitionKeyPattern: { baseDomain: "example.com" },
  });
  testMatch(oa, {
    partitionKey: oa.partitionKey,
    partitionKeyPattern: { baseDomain: "example.net" },
  });
  testMatch(
    oa,
    {
      partitionKey: getPartitionKey("https", "example.net"),
      partitionKeyPattern: { scheme: "https", baseDomain: "example.com" },
    },
    false
  );
});

/**
 * Tests that we can match parts of a partitionKey by setting
 * partitionKeyPattern.
 */
add_task(async function test_pattern() {
  let a = getOAWithPartitionKey("https", "example.com", 8080, {
    userContextId: 2,
  });
  let b = getOAWithPartitionKey("https", "example.com", undefined, {
    privateBrowsingId: 1,
  });

  for (let oa of [a, b]) {
    // Match
    testMatch(oa, { partitionKeyPattern: { scheme: "https" } });
    testMatch(oa, {
      partitionKeyPattern: { scheme: "https", baseDomain: "example.com" },
    });
    testMatch(
      oa,
      {
        partitionKeyPattern: {
          scheme: "https",
          baseDomain: "example.com",
          port: 8080,
        },
      },
      oa == a
    );
    testMatch(oa, {
      partitionKeyPattern: { baseDomain: "example.com" },
    });
    testMatch(
      oa,
      {
        partitionKeyPattern: { port: 8080 },
      },
      oa == a
    );

    // Mismatch
    testMatch(oa, { partitionKeyPattern: { scheme: "http" } }, false);
    testMatch(
      oa,
      { partitionKeyPattern: { baseDomain: "example.net" } },
      false
    );
    testMatch(oa, { partitionKeyPattern: { port: 8443 } }, false);
    testMatch(
      oa,
      { partitionKeyPattern: { scheme: "https", baseDomain: "example.net" } },
      false
    );
  }
});
