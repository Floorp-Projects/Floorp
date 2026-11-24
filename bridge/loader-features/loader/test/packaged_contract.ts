// SPDX-License-Identifier: MPL-2.0

/** Packaged QA never falls back to running every bundled module. */
export function assertPackagedTestSelection(
  runId: string | undefined,
  requested: Set<string>,
  available: Set<string>,
): void {
  if (!runId || requested.size === 0) {
    throw new Error(
      "Packaged QA requires a fresh, nonempty scoped test control.",
    );
  }
  for (const file of requested) {
    if (!available.has(file)) {
      throw new Error(`Requested test is not in this QA package: ${file}`);
    }
  }
}
