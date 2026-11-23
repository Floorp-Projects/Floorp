// SPDX-License-Identifier: MPL-2.0

import type {
  M3cProviderFailureClassification,
} from "./M3cProviderTaxonomy.types.ts";

const candidateErrors = new Set([
  "invalid-envelope",
  "invalid-metadata",
  "invalid-source",
  "invalid-signature",
  "source-digest-mismatch",
  "feature-mismatch",
  "host-incompatible",
  "runtime-incompatible",
  "capability-denied",
  "size-limit",
  "invalid-compatibility",
  "unsupported-version",
  "sequence-too-low",
  "untrusted-key",
  "invalid-package",
  "package-hash-mismatch",
  "source-hash-mismatch",
]);

export function classifyM3cProviderFailure(
  code: string,
): M3cProviderFailureClassification {
  if (code === "cache-missing" || code === "cache-corrupt") {
    return Object.freeze({ kind: "candidate-unavailable" });
  }
  if (code === "cache-storage") {
    return Object.freeze({ kind: "infrastructure", reason: "cache-io" });
  }
  if (code === "cache-unsafe-layout") {
    return Object.freeze({ kind: "infrastructure", reason: "unsafe-layout" });
  }
  if (code === "unsupported-crypto") {
    return Object.freeze({
      kind: "infrastructure",
      reason: "unsupported-crypto",
    });
  }
  if (code === "invalid-policy" || code === "invalid-signing-key") {
    return Object.freeze({ kind: "infrastructure", reason: "cache-contract" });
  }
  if (candidateErrors.has(code)) {
    return Object.freeze({ kind: "candidate-unavailable" });
  }
  if (code.startsWith("cache-")) {
    return Object.freeze({ kind: "infrastructure", reason: "cache-contract" });
  }
  return Object.freeze({ kind: "infrastructure", reason: "unexpected" });
}
