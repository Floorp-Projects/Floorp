// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  sanitizeDNRRules,
  validateDNRRule,
  countInvalidDomains,
} from "./DNRTransformer.sys.mts";

type TestCase = {
  name: string;
  fn: () => void;
};

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) {
    throw new Error(message);
  }
}

function assertEquals<T>(actual: T, expected: T, message: string): void {
  if (actual !== expected) {
    throw new Error(
      `${message} (expected: ${String(expected)}, actual: ${String(actual)})`,
    );
  }
}

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
  const result = sanitizeDNRRules("not-array" as unknown as unknown[]);
  assertEquals(result.length, 0, "non-array should return empty");
}

function testSanitizeRemovesInvalidDomains(): void {
  const rules = [
    makeRule(1, {
      domains: ["example.com", "185.164.59.38.12", "valid.org"],
    }),
  ];
  const result = sanitizeDNRRules(rules);
  const condition = (result[0] as Record<string, unknown>)
    .condition as Record<string, unknown>;
  const domains = condition.domains as string[];
  assert(!domains.includes("185.164.59.38.12"), "invalid IP should be removed");
  assert(domains.includes("example.com"), "valid domain should remain");
  assert(domains.includes("valid.org"), "valid domain should remain");
}

function testSanitizePreservesValidRules(): void {
  const rules = [makeRule(1, { domains: ["example.com"] })];
  const result = sanitizeDNRRules(rules);
  assertEquals(result.length, 1, "valid rule should be preserved");
}

function testSanitizeHandlesNonDNRObjects(): void {
  const rules = [{ some: "random", object: true }, makeRule(1)];
  const result = sanitizeDNRRules(rules);
  assertEquals(result.length, 2, "all items should be in result");
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
      excludedDomains: ["bad..domain", "ok.com"],
    }),
  ];
  assertEquals(
    countInvalidDomains(rules),
    2,
    "should count 2 invalid domains",
  );
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

function testDomainValidationStartsWithDot(): void {
  const rule = makeRule(1, { domains: [".example.com"] });
  const errors = validateDNRRule(rule);
  assert(
    errors.some((e) => e.includes("Invalid domain")),
    "domain starting with dot should fail",
  );
}

function testDomainValidationConsecutiveDots(): void {
  const rule = makeRule(1, { domains: ["example..com"] });
  const errors = validateDNRRule(rule);
  assert(
    errors.some((e) => e.includes("Invalid domain")),
    "consecutive dots should fail",
  );
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
    { name: "sanitize removes invalid domains", fn: testSanitizeRemovesInvalidDomains },
    { name: "sanitize preserves valid rules", fn: testSanitizePreservesValidRules },
    { name: "sanitize handles non-DNR objects", fn: testSanitizeHandlesNonDNRObjects },
    { name: "count invalid domains zero", fn: testCountInvalidDomainsZero },
    { name: "count invalid domains multiple", fn: testCountInvalidDomainsMultiple },
    { name: "count invalid domains empty string", fn: testCountInvalidDomainsEmptyString },
    { name: "IPv4 valid", fn: testDomainValidationIPv4Valid },
    { name: "IPv4 leading zero", fn: testDomainValidationIPv4LeadingZero },
    { name: "hostname with hyphen", fn: testDomainValidationHostnameWithHyphen },
    { name: "domain starts with dot", fn: testDomainValidationStartsWithDot },
    { name: "domain consecutive dots", fn: testDomainValidationConsecutiveDots },
  ];

  const failures: string[] = [];

  for (const test of tests) {
    try {
      test.fn();
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      failures.push(`${test.name}: ${message}`);
    }
  }

  if (failures.length > 0) {
    throw new Error(
      `DNRTransformer.test.mts failures: ${failures.join(" | ")}`,
    );
  }
}
