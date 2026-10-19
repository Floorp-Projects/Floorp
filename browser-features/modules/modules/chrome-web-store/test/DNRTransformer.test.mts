// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  countInvalidDomains,
  sanitizeDNRRules,
  sanitizeDNRRulesFull,
  validateDNRRule,
} from "../DNRTransformer.sys.mts";

import {
  assert,
  assertEquals,
  assertThrows,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

function makeRule(
  id: number,
  condition: Record<string, unknown> = {},
): Record<string, unknown> {
  return {
    id,
    action: { type: "block" },
    condition: { urlFilter: "*", ...condition },
  };
}

// ---------------------------------------------------------------------------
// Tests — validateDNRRule
// ---------------------------------------------------------------------------

function testValidateValidRule(): void {
  const errors = validateDNRRule(makeRule(1));
  assertEquals(errors.length, 0, "valid rule should have no errors");
}

function testValidateNonObject(): void {
  const errors = validateDNRRule("not-an-object");
  assert(errors.length > 0, "non-object should produce errors");
}

function testValidateNull(): void {
  const errors = validateDNRRule(null);
  assert(errors.length > 0, "null should produce errors");
}

function testValidateMissingId(): void {
  const errors = validateDNRRule({
    action: { type: "block" },
    condition: {},
  });
  assert(
    errors.some((e) => e.includes("id")),
    "missing id should be flagged",
  );
}

function testValidateMissingAction(): void {
  const errors = validateDNRRule({ id: 1, condition: {} });
  assert(
    errors.some((e) => e.includes("action")),
    "missing action should be flagged",
  );
}

function testValidateMissingCondition(): void {
  // Without "condition" key, isDNRRule returns false → "Invalid rule structure"
  const errors = validateDNRRule({ id: 1, action: { type: "block" } });
  assert(
    errors.length > 0,
    "missing condition should produce validation errors",
  );
}

function testValidateInvalidDomain(): void {
  const rule = makeRule(1, {
    domains: ["example.com", "185.164.59.38.12"],
  });
  const errors = validateDNRRule(rule);
  assert(
    errors.some((e) => e.includes("185.164.59.38.12")),
    "malformed IP should be flagged",
  );
}

function testValidateValidDomains(): void {
  const rule = makeRule(1, {
    domains: ["example.com", "192.168.1.1"],
  });
  const errors = validateDNRRule(rule);
  const domainErrors = errors.filter((e) => e.includes("domain"));
  assertEquals(
    domainErrors.length,
    0,
    "valid domains should not produce errors",
  );
}

// ---------------------------------------------------------------------------
// Tests — sanitizeDNRRules
// ---------------------------------------------------------------------------

function testSanitizeEmptyArray(): void {
  const result = sanitizeDNRRules([]);
  assertEquals(result.length, 0, "empty array should return empty");
}

function testSanitizeNonArray(): void {
  assertThrows(
    () => sanitizeDNRRulesFull("not-array"),
    "invalid rulesets must not be silently converted to empty arrays",
  );
}

function testSanitizeRejectsInvalidDomains(): void {
  for (
    const field of [
      "domains",
      "excludedDomains",
      "initiatorDomains",
      "excludedInitiatorDomains",
      "requestDomains",
      "excludedRequestDomains",
    ]
  ) {
    const rules = [makeRule(1, {
      [field]: ["example.com", "185.164.59.38.12", "valid.org"],
    })];
    const original = JSON.stringify(rules);
    const result = sanitizeDNRRulesFull(rules);
    assertEquals(result.rules.length, 0, `${field}: reject the complete rule`);
    assertEquals(result.rejectedRules, 1, `${field}: report rejected count`);
    assertEquals(result.rejectedRuleIds[0], 1, `${field}: report rejected ID`);
    assert(
      result.warnings.some((warning) =>
        warning.includes(field) && warning.includes("185.164.59.38.12")
      ),
      `${field}: identify the rejected constraint`,
    );
    assertEquals(JSON.stringify(rules), original, "input must remain intact");
    assertThrows(
      () => sanitizeDNRRules(rules),
      "the legacy API cannot return a partial ruleset without diagnostics",
    );
  }
}

function testSanitizePreservesValidRules(): void {
  const rules = [makeRule(1, {
    initiatorDomains: ["example.com", "[::1]"],
    excludedInitiatorDomains: ["private.example.com"],
    requestDomains: ["cdn.example.com"],
    excludedRequestDomains: ["private.cdn.example.com"],
  })];
  const result = sanitizeDNRRulesFull(rules);
  assertEquals(
    JSON.stringify(result.rules),
    JSON.stringify(rules),
    "modern domain constraints must be preserved verbatim",
  );
  assertEquals(result.warnings.length, 0, "unchanged rules need no warnings");
}

function testSanitizeHandlesNonDNRObjects(): void {
  const rules = [{ some: "random", object: true }, makeRule(1)];
  const result = sanitizeDNRRulesFull(rules);
  assertEquals(result.rejectedRules, 1, "malformed rule must be reported");
  assertEquals(result.rejectedRuleIds.length, 0, "no fabricated numeric ID");
  assert(
    result.warnings.some((warning) => warning.includes("index 0")),
    "malformed objects must be identified by their ruleset index",
  );
}

function testSanitizeMigratesLegacyDomains(): void {
  const rules = [makeRule(1, {
    domains: ["example.com"],
    excludedDomains: ["private.example.com"],
  })];
  const original = JSON.stringify(rules);
  const result = sanitizeDNRRulesFull(rules);
  assertEquals(
    JSON.stringify(result.rules),
    JSON.stringify([makeRule(1, {
      initiatorDomains: ["example.com"],
      excludedInitiatorDomains: ["private.example.com"],
    })]),
    "deprecated aliases must retain inclusion and exclusion semantics",
  );
  assertEquals(result.warnings.length, 2, "each alias migration is recorded");
  assertEquals(result.rejectedRules, 0, "valid aliases should be supported");
  assertEquals(
    JSON.stringify(rules),
    original,
    "migration must not mutate input",
  );
}

function testSanitizeRejectsConflictingAliases(): void {
  for (
    const [legacy, modern] of [
      ["domains", "initiatorDomains"],
      ["excludedDomains", "excludedInitiatorDomains"],
    ]
  ) {
    const result = sanitizeDNRRulesFull([makeRule(1, {
      [legacy]: ["example.com"],
      [modern]: ["other.com"],
    })]);
    assertEquals(
      result.rejectedRules,
      1,
      "conflicting aliases must be rejected",
    );
    assert(
      result.warnings.some((warning) =>
        warning.includes("Cannot specify both")
      ),
      "conflict must be explained",
    );
  }
}

function testSanitizeRejectsUnsupportedConditions(): void {
  const result = sanitizeDNRRulesFull([makeRule(1, {
    unknownHeaderCondition: [{ header: "content-type", values: ["image/*"] }],
  })]);
  assertEquals(
    result.rejectedRules,
    1,
    "unsupported constraints must reject rule",
  );
  assert(
    result.warnings.some((warning) =>
      warning.includes("Unsupported condition: unknownHeaderCondition")
    ),
    "unsupported condition name must be reported",
  );
}

function testPreservesResponseHeaderConditions(): void {
  const rules = [
    makeRule(1, {
      responseHeaders: [
        {
          header: "content-type",
          values: ["text/*"],
          excludedValues: ["text/html*"],
        },
      ],
      excludedResponseHeaders: [{ header: "x-excluded" }],
    }),
  ];
  const result = sanitizeDNRRulesFull(rules);
  assertEquals(
    result.rejectedRules,
    0,
    "native response header constraints remain supported",
  );
  assertEquals(
    JSON.stringify(result.rules),
    JSON.stringify(rules),
    "response header constraints must remain byte-for-byte equivalent",
  );
  for (
    const condition of [
      { responseHeaders: [] },
      { excludedResponseHeaders: [] },
      { responseHeaders: [{ header: "invalid name" }] },
    { responseHeaders: [{ header: "x-test" }], excludedResponseHeaders: [{ header: "x-test" }] },
      { responseHeaders: [{ header: "x-test", values: ["bad\r\nvalue"] }] },
      { responseHeaders: [{ header: "x-test", unknownConstraint: true }] },
    ]
  ) {
    assertEquals(
      sanitizeDNRRulesFull([makeRule(1, condition)]).rejectedRules,
      1,
      "invalid header matching constraints must reject rather than broaden rules",
    );
  }
}


function testSanitizeRejectsDuplicateIds(): void {
  const result = sanitizeDNRRulesFull([makeRule(1), makeRule(1)]);
  assertEquals(result.rejectedRules, 1, "duplicate IDs must be reported");
  assert(
    result.warnings.some((warning) => warning.includes("Duplicate rule id: 1")),
    "the duplicate ID must be identified",
  );
}

function testSanitizeRejectsInvalidDomainLists(): void {
  for (
    const condition of [
      { requestDomains: "example.com" },
      { requestDomains: [] },
      { initiatorDomains: [] },
      { excludedRequestDomains: null },
      { requestDomains: [1] },
    ]
  ) {
    assertEquals(
      sanitizeDNRRulesFull([makeRule(1, condition)]).rejectedRules,
      1,
      "malformed domain lists must not lose their constraints",
    );
  }
  assertEquals(
    sanitizeDNRRulesFull([makeRule(1, { excludedRequestDomains: [] })])
      .rejectedRules,
    0,
    "empty exclusion lists are supported by the native schema",
  );
}

function testSanitizeRejectsInvalidStructure(): void {
  for (
    const rule of [
      { id: 0, action: { type: "block" }, condition: {} },
      { id: 1.5, action: { type: "block" }, condition: {} },
      { id: 1, priority: 0, action: { type: "block" }, condition: {} },
      { id: 1, action: [], condition: {} },
      { id: 1, action: { type: "block" }, condition: [] },
    ]
  ) {
    assertEquals(
      sanitizeDNRRulesFull([rule]).rejectedRules,
      1,
      "native rule structure constraints must be respected",
    );
  }
}

// ---------------------------------------------------------------------------
// Tests — countInvalidDomains
// ---------------------------------------------------------------------------

function testCountInvalidDomainsZero(): void {
  const rules = [makeRule(1, { domains: ["example.com"] })];
  assertEquals(countInvalidDomains(rules), 0, "should count zero invalid");
}

function testCountInvalidDomainsMultiple(): void {
  const rules = [
    makeRule(1, {
      domains: ["example.com", "185.164.59.38.12"],
      excludedDomains: ["bad domain", "ok.com"],
    }),
  ];
  assertEquals(countInvalidDomains(rules), 2, "should count 2 invalid domains");
}

function testCountInvalidDomainsEmptyString(): void {
  const rules = [makeRule(1, { domains: ["", "example.com"] })];
  assertEquals(
    countInvalidDomains(rules),
    1,
    "empty string should be counted as invalid",
  );
}

// ---------------------------------------------------------------------------
// Domain validation edge cases (tested indirectly)
// ---------------------------------------------------------------------------

function testDomainValidationIPv4Valid(): void {
  const rule = makeRule(1, { domains: ["192.168.1.1"] });
  const errors = validateDNRRule(rule);
  const domainErrors = errors.filter((e) => e.includes("Invalid domain"));
  assertEquals(domainErrors.length, 0, "valid IPv4 should pass");
}

function testDomainValidationIPv4LeadingZero(): void {
  const rule = makeRule(1, { domains: ["192.168.01.1"] });
  const errors = validateDNRRule(rule);
  assert(
    errors.some((e) => e.includes("Invalid domain")),
    "IPv4 with leading zero should fail",
  );
}

function testDomainValidationHostnameWithHyphen(): void {
  const rule = makeRule(1, { domains: ["my-domain.com"] });
  const errors = validateDNRRule(rule);
  const domainErrors = errors.filter((e) => e.includes("Invalid domain"));
  assertEquals(domainErrors.length, 0, "hostname with hyphen should pass");
}

function testDomainValidationCanonicalNativeDomains(): void {
  // These follow the pinned Runtime's canonicalDomain schema. Do not replace
  // native URL parsing with DNS hostname heuristics or drop IPv6 constraints.
  for (const domain of ["[::1]", "[::1a]", "lowercase", ".", ".example.com"]) {
    const errors = validateDNRRule(makeRule(1, { requestDomains: [domain] }));
    assertEquals(
      errors.length,
      0,
      `native canonical domain ${domain} is valid`,
    );
  }
}

function testDomainValidationNoncanonicalDomains(): void {
  for (
    const domain of [
      "example.com ",
      "UPPERCASE",
      "straß.de",
      "[::1A]",
      "[::ffff:127.0.0.1]",
      "example.com:80",
      "example.com/path",
      "::1",
      "2",
      "%61",
    ]
  ) {
    const result = sanitizeDNRRulesFull([makeRule(1, {
      requestDomains: [domain],
    })]);
    assertEquals(result.rejectedRules, 1, `${domain} must not be rewritten`);
  }
}

// ---------------------------------------------------------------------------
// Runner
// ---------------------------------------------------------------------------

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "validate valid rule", fn: testValidateValidRule },
    { name: "validate non-object", fn: testValidateNonObject },
    { name: "validate null", fn: testValidateNull },
    { name: "validate missing id", fn: testValidateMissingId },
    { name: "validate missing action", fn: testValidateMissingAction },
    { name: "validate missing condition", fn: testValidateMissingCondition },
    { name: "validate invalid domain", fn: testValidateInvalidDomain },
    { name: "validate valid domains", fn: testValidateValidDomains },
    { name: "sanitize empty array", fn: testSanitizeEmptyArray },
    { name: "sanitize non-array", fn: testSanitizeNonArray },
    {
      name: "sanitize rejects invalid domain constraints",
      fn: testSanitizeRejectsInvalidDomains,
    },
    {
      name: "sanitize preserves valid rules",
      fn: testSanitizePreservesValidRules,
    },
    {
      name: "sanitize handles non-DNR objects",
      fn: testSanitizeHandlesNonDNRObjects,
    },
    {
      name: "sanitize migrates deprecated domain aliases",
      fn: testSanitizeMigratesLegacyDomains,
    },
    {
      name: "sanitize rejects conflicting aliases",
      fn: testSanitizeRejectsConflictingAliases,
    },
    {
      name: "sanitize preserves supported response header constraints",
      fn: testPreservesResponseHeaderConditions,
    },
    {
      name: "sanitize rejects unsupported constraints",
      fn: testSanitizeRejectsUnsupportedConditions,
    },
    {
      name: "sanitize rejects duplicate rule IDs",
      fn: testSanitizeRejectsDuplicateIds,
    },
    {
      name: "sanitize rejects malformed domain lists",
      fn: testSanitizeRejectsInvalidDomainLists,
    },
    {
      name: "sanitize rejects invalid rule structures",
      fn: testSanitizeRejectsInvalidStructure,
    },
    { name: "count invalid domains zero", fn: testCountInvalidDomainsZero },
    {
      name: "count invalid domains multiple",
      fn: testCountInvalidDomainsMultiple,
    },
    {
      name: "count invalid domains empty string",
      fn: testCountInvalidDomainsEmptyString,
    },
    { name: "IPv4 valid", fn: testDomainValidationIPv4Valid },
    { name: "IPv4 leading zero", fn: testDomainValidationIPv4LeadingZero },
    {
      name: "hostname with hyphen",
      fn: testDomainValidationHostnameWithHyphen,
    },
    {
      name: "native canonical domains including IPv6",
      fn: testDomainValidationCanonicalNativeDomains,
    },
    {
      name: "noncanonical domains rejected without rewriting",
      fn: testDomainValidationNoncanonicalDomains,
    },
  ];

  await runTests("DNRTransformer.test.mts", tests);
}
