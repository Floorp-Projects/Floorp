/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * DNR Transformer - Sanitizes Declarative Net Request rules for Firefox compatibility
 */

interface DNRRule {
  id: number;
  priority?: number;
  action: Record<string, unknown>;
  condition: DNRRuleCondition;
}

interface DNRRuleCondition {
  urlFilter?: string;
  regexFilter?: string;
  isUrlFilterCaseSensitive?: boolean;
  domains?: string[];
  excludedDomains?: string[];
  initiatorDomains?: string[];
  excludedInitiatorDomains?: string[];
  requestDomains?: string[];
  excludedRequestDomains?: string[];
  requestMethods?: string[];
  excludedRequestMethods?: string[];
  resourceTypes?: string[];
  excludedResourceTypes?: string[];
  tabIds?: number[];
  excludedTabIds?: number[];
  [key: string]: unknown;
}

/**
 * Sanitize DNR rules to remove invalid domains that cause errors in Firefox
 */
export function sanitizeDNRRules(rules: unknown[]): unknown[] {
  if (!Array.isArray(rules)) {
    return [];
  }

  return rules.map((rule) => {
    if (!isDNRRule(rule)) {
      return rule;
    }

    const sanitizedRule = { ...rule };
    const condition = { ...rule.condition };

    // List of domain-related fields to sanitize
    const domainFields = [
      "domains",
      "excludedDomains",
      "initiatorDomains",
      "excludedInitiatorDomains",
      "requestDomains",
      "excludedRequestDomains",
    ] as const;

    let modified = false;

    for (const field of domainFields) {
      if (Array.isArray(condition[field])) {
        const originalLength = condition[field]!.length;
        const validDomains = condition[field]!.filter(isValidDomain);

        if (validDomains.length !== originalLength) {
          condition[field] = validDomains;
          modified = true;
          console.log(
            `[DNRTransformer] Removed ${
              originalLength - validDomains.length
            } invalid domain(s) from rule ${rule.id} field ${field}`,
          );
        }
      }
    }

    if (modified) {
      sanitizedRule.condition = condition;
    }

    return sanitizedRule;
  });
}

/**
 * Check if a value is a valid DNR rule object
 */
function isDNRRule(value: unknown): value is DNRRule {
  return (
    typeof value === "object" &&
    value !== null &&
    "id" in value &&
    "condition" in value &&
    typeof (value as DNRRule).condition === "object"
  );
}

/**
 * Validate domain format
 * Firefox is stricter than Chrome about domain formats in DNR rules
 * Specifically rejects malformed IPs like "185.164.59.38.12"
 */
function isValidDomain(domain: string): boolean {
  if (!domain || typeof domain !== "string") {
    return false;
  }

  // Remove leading/trailing whitespace
  domain = domain.trim();

  // Check for empty string
  if (domain.length === 0) {
    return false;
  }

  // Check for malformed IP addresses (too many octets)
  // Matches 4+ dots with numbers around them, e.g. 1.2.3.4.5
  if (/^(\d{1,3}\.){4,}\d{1,3}$/.test(domain)) {
    return false;
  }

  // Basic domain validation
  // - Must not start or end with dot (unless it's just a dot? no, domains don't work like that here)
  // - Must not contain spaces
  // - Allowed chars: a-z, 0-9, -, ., * (wildcards are allowed in some contexts, but let's be careful)
  // Note: DNR domains usually don't allow wildcards except maybe at start?
  // Chrome docs say: "The domain must be a valid domain name (e.g. "example.com")."
  // It doesn't explicitly say wildcards are allowed in `requestDomains`.

  // If it looks like an IP address, validate it strictly
  if (/^[\d.]+$/.test(domain)) {
    // Check if it's a valid IPv4
    const parts = domain.split(".");
    if (parts.length !== 4) {
      return false;
    }
    return parts.every((part) => {
      const num = parseInt(part, 10);
      return num >= 0 && num <= 255;
    });
  }

  return true;
}
