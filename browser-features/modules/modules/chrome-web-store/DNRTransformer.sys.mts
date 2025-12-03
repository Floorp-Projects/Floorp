/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * DNR Transformer - Sanitizes Declarative Net Request rules for Firefox compatibility
 */

import type { DNRRule } from "./types.ts";

// =============================================================================
// Constants
// =============================================================================

/**
 * Domain-related fields that need sanitization
 */
const DOMAIN_FIELDS = [
  "domains",
  "excludedDomains",
  "initiatorDomains",
  "excludedInitiatorDomains",
  "requestDomains",
  "excludedRequestDomains",
] as const;

// =============================================================================
// Public API
// =============================================================================

/**
 * Sanitize DNR rules to remove invalid domains that cause errors in Firefox
 * @param rules - Array of DNR rules to sanitize
 * @returns Sanitized rules array
 */
export function sanitizeDNRRules(rules: unknown[]): unknown[] {
  if (!Array.isArray(rules)) {
    console.warn(
      "[DNRTransformer] Input is not an array, returning empty array",
    );
    return [];
  }

  const results: unknown[] = [];
  let totalRemoved = 0;

  for (const rule of rules) {
    if (!isDNRRule(rule)) {
      results.push(rule);
      continue;
    }

    const { sanitizedRule, removedCount } = sanitizeRule(rule);
    results.push(sanitizedRule);
    totalRemoved += removedCount;
  }

  if (totalRemoved > 0) {
    console.log(
      `[DNRTransformer] Removed ${totalRemoved} invalid domain(s) from ${rules.length} rule(s)`,
    );
  }

  return results;
}

/**
 * Validate a single DNR rule
 * @param rule - Rule to validate
 * @returns Array of validation errors (empty if valid)
 */
export function validateDNRRule(rule: unknown): string[] {
  const errors: string[] = [];

  if (!isDNRRule(rule)) {
    errors.push("Invalid rule structure");
    return errors;
  }

  // Check for required fields
  if (typeof rule.id !== "number") {
    errors.push("Rule must have a numeric id");
  }

  if (!rule.action || typeof rule.action !== "object") {
    errors.push("Rule must have an action object");
  }

  if (!rule.condition || typeof rule.condition !== "object") {
    errors.push("Rule must have a condition object");
  }

  // Validate domains in condition
  for (const field of DOMAIN_FIELDS) {
    const domains = rule.condition[field];
    if (Array.isArray(domains)) {
      for (const domain of domains) {
        if (!isValidDomain(domain)) {
          errors.push(`Invalid domain in ${field}: ${domain}`);
        }
      }
    }
  }

  return errors;
}

/**
 * Count the number of invalid domains in a rule set
 * @param rules - Array of DNR rules
 * @returns Count of invalid domains
 */
export function countInvalidDomains(rules: unknown[]): number {
  let count = 0;

  for (const rule of rules) {
    if (!isDNRRule(rule)) continue;

    for (const field of DOMAIN_FIELDS) {
      const domains = rule.condition[field];
      if (Array.isArray(domains)) {
        count += domains.filter((d) => !isValidDomain(d)).length;
      }
    }
  }

  return count;
}

// =============================================================================
// Internal Functions
// =============================================================================

/**
 * Sanitize a single DNR rule
 */
function sanitizeRule(rule: DNRRule): {
  sanitizedRule: DNRRule;
  removedCount: number;
} {
  const sanitizedRule = { ...rule };
  const condition = { ...rule.condition };
  let removedCount = 0;

  for (const field of DOMAIN_FIELDS) {
    const domains = condition[field];
    if (!Array.isArray(domains)) continue;

    const originalLength = domains.length;
    const validDomains = domains.filter(isValidDomain);

    if (validDomains.length !== originalLength) {
      const removed = originalLength - validDomains.length;
      removedCount += removed;

      // Log the removed domains for debugging
      const invalidDomains = domains.filter((d) => !isValidDomain(d));
      console.debug(
        `[DNRTransformer] Rule ${rule.id}: removed ${removed} invalid domain(s) from ${field}:`,
        invalidDomains,
      );

      condition[field] = validDomains;
    }
  }

  if (removedCount > 0) {
    sanitizedRule.condition = condition;
  }

  return { sanitizedRule, removedCount };
}

/**
 * Check if a value is a valid DNR rule object
 */
function isDNRRule(value: unknown): value is DNRRule {
  if (typeof value !== "object" || value === null) {
    return false;
  }

  const obj = value as Record<string, unknown>;
  return (
    "id" in obj &&
    typeof obj.id === "number" &&
    "condition" in obj &&
    typeof obj.condition === "object" &&
    obj.condition !== null
  );
}

/**
 * Validate domain format
 * Firefox is stricter than Chrome about domain formats in DNR rules
 * @param domain - Domain string to validate
 */
function isValidDomain(domain: unknown): boolean {
  if (typeof domain !== "string" || domain.length === 0) {
    return false;
  }

  // Trim whitespace
  const trimmed = domain.trim();
  if (trimmed.length === 0) {
    return false;
  }

  // Check for malformed IP addresses (too many octets)
  // e.g., "185.164.59.38.12" is invalid
  if (/^(\d{1,3}\.){4,}\d{1,3}$/.test(trimmed)) {
    return false;
  }

  // If it looks like an IP address, validate it strictly
  if (/^[\d.]+$/.test(trimmed)) {
    return isValidIPv4(trimmed);
  }

  // Basic hostname validation
  // - Must not start or end with dot or hyphen
  // - Must not contain consecutive dots
  // - Must not contain spaces or invalid characters
  if (
    trimmed.startsWith(".") ||
    trimmed.endsWith(".") ||
    trimmed.startsWith("-") ||
    trimmed.endsWith("-") ||
    trimmed.includes("..") ||
    /\s/.test(trimmed) ||
    !/^[a-z0-9.-]+$/i.test(trimmed)
  ) {
    return false;
  }

  return true;
}

/**
 * Validate IPv4 address format
 */
function isValidIPv4(ip: string): boolean {
  const parts = ip.split(".");

  if (parts.length !== 4) {
    return false;
  }

  return parts.every((part) => {
    if (part.length === 0 || part.length > 3) {
      return false;
    }

    // No leading zeros (except for "0" itself)
    if (part.length > 1 && part.startsWith("0")) {
      return false;
    }

    const num = parseInt(part, 10);
    return !isNaN(num) && num >= 0 && num <= 255;
  });
}
