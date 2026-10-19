/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/** Conservative conversion of static Declarative Net Request rules. */

const DOMAIN_FIELDS = [
  "domains",
  "excludedDomains",
  "initiatorDomains",
  "excludedInitiatorDomains",
  "requestDomains",
  "excludedRequestDomains",
] as const;

const DOMAIN_ALIASES = {
  domains: "initiatorDomains",
  excludedDomains: "excludedInitiatorDomains",
} as const;

// Keep this aligned with the pinned Runtime's declarative_net_request.json.
// Unknown conditions must not disappear during native schema normalization:
// dropping a constraint can broaden a block, redirect, or allow rule.
const CONDITION_FIELDS = new Set([
  ...DOMAIN_FIELDS,
  "urlFilter",
  "regexFilter",
  "isUrlFilterCaseSensitive",
  "resourceTypes",
  "excludedResourceTypes",
  "requestMethods",
  "excludedRequestMethods",
  "domainType",
  "tabIds",
  "excludedTabIds",
  "responseHeaders",
  "excludedResponseHeaders",
]);

export interface DNRSanitizationResult {
  rules: unknown[];
  warnings: string[];
  rejectedRuleIds: number[];
  rejectedRules: number;
}

/**
 * Convert rules without discarding matching constraints. Callers that need
 * diagnostics should use sanitizeDNRRulesFull instead.
 */
export function sanitizeDNRRules(rules: unknown[]): unknown[] {
  const result = sanitizeDNRRulesFull(rules);
  if (result.rejectedRules > 0) {
    throw new TypeError(result.warnings.join("; "));
  }
  return result.rules;
}

/**
 * Return compatible rules and explicit diagnostics for every rejection or
 * legacy-field migration. Rejected rules must prevent package installation;
 * this result is not an authorization to install a partial ruleset.
 */
export function sanitizeDNRRulesFull(rules: unknown): DNRSanitizationResult {
  if (!Array.isArray(rules)) {
    throw new TypeError("DNR ruleset must be an array");
  }

  const result: DNRSanitizationResult = {
    rules: [],
    warnings: [],
    rejectedRuleIds: [],
    rejectedRules: 0,
  };
  const seenIds = new Set<number>();

  for (const [index, rule] of rules.entries()) {
    const errors = validateDNRRule(rule);
    const id = isRecord(rule) && typeof rule.id === "number"
      ? rule.id
      : undefined;
    if (id !== undefined && seenIds.has(id)) {
      errors.push(`Duplicate rule id: ${id}`);
    }

    if (errors.length > 0) {
      result.rejectedRules++;
      if (id !== undefined) result.rejectedRuleIds.push(id);
      result.warnings.push(
        `Rejected DNR rule ${id ?? `at index ${index}`}: ${errors.join("; ")}`,
      );
      continue;
    }

    // Validation above established these shapes. Preserve all values and only
    // rename the two deprecated Chrome aliases with identical semantics.
    const validRule = rule as Record<string, unknown>;
    const condition = validRule.condition as Record<string, unknown>;
    const migratedCondition = { ...condition };
    let migrated = false;
    for (const [legacy, modern] of Object.entries(DOMAIN_ALIASES)) {
      if (legacy in condition) {
        migratedCondition[modern] = condition[legacy];
        delete migratedCondition[legacy];
        migrated = true;
        result.warnings.push(`DNR rule ${id}: mapped ${legacy} to ${modern}`);
      }
    }
    seenIds.add(id as number);
    result.rules.push(
      migrated ? { ...validRule, condition: migratedCondition } : rule,
    );
  }

  return result;
}

/**
 * Validate conversion-sensitive structure and domain conditions. Gecko remains
 * responsible for complete action, regex, and native rule-limit validation.
 */
export function validateDNRRule(rule: unknown): string[] {
  if (!isRecord(rule)) return ["Invalid rule structure"];

  const errors: string[] = [];
  if (
    typeof rule.id !== "number" || !Number.isInteger(rule.id) || rule.id < 1
  ) {
    errors.push("Rule must have a positive integer id");
  }
  if (
    rule.priority !== undefined &&
    (typeof rule.priority !== "number" ||
      !Number.isInteger(rule.priority) || rule.priority < 1)
  ) {
    errors.push("Rule priority must be a positive integer");
  }
  if (!isRecord(rule.action) || typeof rule.action.type !== "string") {
    errors.push("Rule must have an action object with a type");
  }
  if (!isRecord(rule.condition)) {
    errors.push("Rule must have a condition object");
    return errors;
  }

  const condition = rule.condition;
  for (const field of Object.keys(condition)) {
    if (!CONDITION_FIELDS.has(field)) {
      errors.push(`Unsupported condition: ${field}`);
    }
  }
  for (const [legacy, modern] of Object.entries(DOMAIN_ALIASES)) {
    if (legacy in condition && modern in condition) {
      errors.push(`Cannot specify both ${legacy} and ${modern}`);
    }
  }
  for (const field of DOMAIN_FIELDS) {
    if (!(field in condition)) continue;
    const domains = condition[field];
    if (!Array.isArray(domains)) {
      errors.push(`${field} must be an array`);
      continue;
    }
    if (!field.startsWith("excluded") && domains.length === 0) {
      errors.push(`${field} must not be empty`);
    }
    for (const domain of domains) {
      if (!isValidDomain(domain)) {
        errors.push(`Invalid domain in ${field}: ${String(domain)}`);
      }
    }
  }

  for (const field of ["responseHeaders", "excludedResponseHeaders"]) {
    if (!(field in condition)) continue;
    const headers = condition[field];
    if (!Array.isArray(headers) || headers.length === 0) {
      errors.push(`${field} must be a non-empty array`);
      continue;
    }
    for (const info of headers) {
      if (
        !isRecord(info) ||
        typeof info.header !== "string" ||
        !/^[!#$%&'*+.^_`|~0-9A-Za-z-]+$/.test(info.header)
      ) {
        errors.push(`Invalid header in ${field}`);
        continue;
      }
      for (const key of Object.keys(info)) {
        if (!["header", "values", "excludedValues"].includes(key)) {
          errors.push(`Unsupported HeaderInfo field: ${key}`);
        }
      }
      for (const key of ["values", "excludedValues"]) {
        if (!(key in info)) continue;
        const values = info[key];
        if (
          !Array.isArray(values) ||
          values.some(
            (value) => typeof value !== "string" || /[\r\n\0]/.test(value),
          )
        ) {
          errors.push(`Invalid ${key} in ${field}`);
        }
      }
    }
  }
  if (Array.isArray(condition.responseHeaders) && Array.isArray(condition.excludedResponseHeaders)) {
    const names = new Set(condition.responseHeaders.filter(isRecord).map(info => info.header));
    if (condition.excludedResponseHeaders.some(info => isRecord(info) && !("values" in info) && !("excludedValues" in info) && names.has(info.header))) {
      errors.push("A response header cannot be required and excluded by name");
    }
  }
  if (
    (condition.responseHeaders || condition.excludedResponseHeaders) &&
    isRecord(rule.action) &&
    "requestHeaders" in rule.action
  ) {
    errors.push("Response header conditions cannot modify request headers");
  }


  return errors;
}

/** Count invalid domain values without changing their matching constraints. */
export function countInvalidDomains(rules: unknown[]): number {
  let count = 0;
  for (const rule of rules) {
    if (!isRecord(rule) || !isRecord(rule.condition)) continue;
    for (const field of DOMAIN_FIELDS) {
      const domains = rule.condition[field];
      if (Array.isArray(domains)) {
        count += domains.filter((domain) => !isValidDomain(domain)).length;
      }
    }
  }
  return count;
}

function isRecord(value: unknown): value is Record<string, unknown> {
  return typeof value === "object" && value !== null && !Array.isArray(value);
}

/** Match the Runtime Schemas.sys.mjs canonicalDomain format exactly. */
function isValidDomain(domain: unknown): boolean {
  if (typeof domain !== "string" || domain.length === 0) return false;
  try {
    return new URL(`http://${domain}`).hostname === domain;
  } catch {
    return false;
  }
}
