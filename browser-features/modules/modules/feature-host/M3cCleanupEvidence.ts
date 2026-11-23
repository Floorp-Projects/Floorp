// SPDX-License-Identifier: MPL-2.0

export function recordM3cCleanupEvidence(
  evidence: string[],
  reason: string,
): void {
  if (evidence.length < 16 && !evidence.includes(reason)) {
    evidence.push(reason.slice(0, 96));
  }
}
