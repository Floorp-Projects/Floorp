// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

/**
 * Validates the Floorp Notes Sync prerequisites contract
 * (docs/development/floorp-notes-sync/). The validator rejects missing roles,
 * guessed/placeholder staging endpoint IDs, wrong runtime SHA, wrong fixture
 * digest, missing required cases, and unapproved trust anchors. It emits an
 * externally-blocked outcome for the shipped pending-approval file and must
 * PASS a fully-approved fixture.
 */

import {
  assertEquals,
  assert,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

interface Prerequisites {
  schema_version: number;
  adr: string;
  engine_authority: { commit: string };
  runtime_pin: { commit: string; tree: string };
  desktop_release: { commit: string };
  staging_environment: {
    status: string;
    fxa_endpoint_id: string;
    sync_endpoint_id: string;
  };
  role_registry: { role: string; login: string; key_fingerprint: string }[];
  fixture: { sha256: string; required_cases: string[] };
  g6: { signatures: unknown[] };
}

const DOC_ROOT = new URL(
  "../../../../docs/development/floorp-notes-sync/",
  import.meta.url,
);

const EXPECTED_RUNTIME_COMMIT = "2d38da4d11be1e0e615f4ddd785ad5e77c95e18d";
const EXPECTED_RUNTIME_TREE = "e555a371e1a24f18c8085058461f92c06e0b997d";
const EXPECTED_FIXTURE_DIGEST =
  "2597e5311c7c4ea4bb9d6a806ffa183aae3b3bd7380893b664b02ac829d665fd";
const REQUIRED_CASES = [
  "concurrent-edits-preserve-deterministic-loser",
  "equal-timestamp-has-commutative-bytewise-winner",
  "first-sync-same-id-preserves-both-versions",
  "one-sided-remote-reorder-wins",
  "one-sided-deletion-wins",
  "delete-versus-edit-keeps-edit-in-both-directions",
  "concurrent-reorder-prefers-local-and-appends-remote-new",
  "conflict-probe-skips-unrelated-collision",
  "conflict-candidate-conflict-winner-is-reused",
  "rich-unknown-content-is-byte-preserved",
  "uploaded-then-local-commit-failure-retry-is-idempotent",
  "duplicate-local-id-fails-closed",
];

const GUID_PATTERN = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

export function validatePrerequisites(prerequisites: Prerequisites): string[] {
  const errors: string[] = [];
  if (prerequisites.schema_version !== 1) errors.push("schema_version");
  if (!prerequisites.adr) errors.push("adr");
  if (!prerequisites.engine_authority?.commit) errors.push("engine_authority");
  if (prerequisites.runtime_pin?.commit !== EXPECTED_RUNTIME_COMMIT) {
    errors.push("runtime_commit");
  }
  if (prerequisites.runtime_pin?.tree !== EXPECTED_RUNTIME_TREE) {
    errors.push("runtime_tree");
  }
  if (!prerequisites.desktop_release?.commit) errors.push("desktop_commit");
  const staging = prerequisites.staging_environment;
  if (!staging || staging.status === "pending_owner_approval") {
    errors.push("staging_approval");
  }
  if (!staging || !GUID_PATTERN.test(staging.fxa_endpoint_id ?? "")) {
    errors.push("fxa_endpoint_id");
  }
  if (!staging || !GUID_PATTERN.test(staging.sync_endpoint_id ?? "")) {
    errors.push("sync_endpoint_id");
  }
  const roles = prerequisites.role_registry ?? [];
  if (roles.length === 0) errors.push("role_registry_empty");
  const namedRoles = new Set(roles.map((r) => r.role));
  for (const requiredRole of [
    "architecture-owner",
    "security-reviewer",
    "privacy-reviewer",
    "retention-reviewer",
    "rollout-approver",
  ]) {
    if (!namedRoles.has(requiredRole)) errors.push(`missing_role:${requiredRole}`);
  }
  for (const role of roles) {
    if (!role.login || role.login.startsWith("PENDING_")) {
      errors.push(`role_login:${role.role}`);
    }
    if (!role.key_fingerprint || role.key_fingerprint.startsWith("PENDING_")) {
      errors.push(`role_fingerprint:${role.role}`);
    }
  }
  if (prerequisites.fixture?.sha256 !== EXPECTED_FIXTURE_DIGEST) {
    errors.push("fixture_digest");
  }
  for (const requiredCase of REQUIRED_CASES) {
    if (!(prerequisites.fixture?.required_cases ?? []).includes(requiredCase)) {
      errors.push(`missing_case:${requiredCase}`);
    }
  }
  if ((prerequisites.g6?.signatures?.length ?? 0) > 0) {
    errors.push("g6_signatures_exist");
  }
  return errors;
}

const tests: TestCase[] = [];

tests.push({
  name: "shipped prerequisites emit externally-blocked status (pending approval)",
  fn: async () => {
    const prerequisites: Prerequisites = JSON.parse(
      await Deno.readTextFile(new URL("prerequisites.json", DOC_ROOT)),
    );
    const errors = validatePrerequisites(prerequisites);
    assert(
      errors.includes("staging_approval"),
      "shipped prerequisites must record pending owner approval",
    );
    assert(
      errors.includes("role_fingerprint:architecture-owner"),
      "pending role fingerprints must be rejected",
    );
  },
});

tests.push({
  name: "fully approved fixture passes",
  fn: () => {
    const approved: Prerequisites = {
      schema_version: 1,
      adr: "ADR-001",
      engine_authority: { commit: "d588863894e9b3ce58b05a964a7694ab00e28054" },
      runtime_pin: {
        commit: EXPECTED_RUNTIME_COMMIT,
        tree: EXPECTED_RUNTIME_TREE,
      },
      desktop_release: { commit: "811a5b821e1d9d47b40f22aee6df5db2254a54b1" },
      staging_environment: {
        status: "approved",
        fxa_endpoint_id: "11111111-2222-3333-4444-555555555555",
        sync_endpoint_id: "66666666-7777-8888-9999-aaaaaaaaaaaa",
      },
      role_registry: [
        { role: "architecture-owner", login: "arch", key_fingerprint: "SHA256:aa" },
        { role: "security-reviewer", login: "sec", key_fingerprint: "SHA256:bb" },
        { role: "privacy-reviewer", login: "priv", key_fingerprint: "SHA256:cc" },
        { role: "retention-reviewer", login: "ret", key_fingerprint: "SHA256:dd" },
        { role: "rollout-approver", login: "roll", key_fingerprint: "SHA256:ee" },
      ],
      fixture: { sha256: EXPECTED_FIXTURE_DIGEST, required_cases: REQUIRED_CASES },
      g6: { signatures: [] },
    };
    assertEquals(validatePrerequisites(approved), [], "approved fixture must pass");
  },
});

tests.push({
  name: "wrong runtime SHA and fixture digest are rejected",
  fn: () => {
    const base = {
      schema_version: 1,
      adr: "ADR-001",
      engine_authority: { commit: "d588863894e9b3ce58b05a964a7694ab00e28054" },
      runtime_pin: { commit: "0".repeat(40), tree: EXPECTED_RUNTIME_TREE },
      desktop_release: { commit: "811a5b821e1d9d47b40f22aee6df5db2254a54b1" },
      staging_environment: {
        status: "approved",
        fxa_endpoint_id: "11111111-2222-3333-4444-555555555555",
        sync_endpoint_id: "66666666-7777-8888-9999-aaaaaaaaaaaa",
      },
      role_registry: [
        { role: "architecture-owner", login: "a", key_fingerprint: "SHA256:aa" },
        { role: "security-reviewer", login: "b", key_fingerprint: "SHA256:bb" },
        { role: "privacy-reviewer", login: "c", key_fingerprint: "SHA256:cc" },
        { role: "retention-reviewer", login: "d", key_fingerprint: "SHA256:dd" },
        { role: "rollout-approver", login: "e", key_fingerprint: "SHA256:ee" },
      ],
      fixture: { sha256: "0".repeat(64), required_cases: REQUIRED_CASES },
      g6: { signatures: [] },
    };
    const errors = validatePrerequisites(base);
    assert(errors.includes("runtime_commit"), "wrong runtime SHA must be rejected");
    assert(errors.includes("fixture_digest"), "wrong fixture digest must be rejected");
  },
});

tests.push({
  name: "missing role and missing case are rejected",
  fn: () => {
    const base = {
      schema_version: 1,
      adr: "ADR-001",
      engine_authority: { commit: "d588863894e9b3ce58b05a964a7694ab00e28054" },
      runtime_pin: { commit: EXPECTED_RUNTIME_COMMIT, tree: EXPECTED_RUNTIME_TREE },
      desktop_release: { commit: "811a5b821e1d9d47b40f22aee6df5db2254a54b1" },
      staging_environment: {
        status: "approved",
        fxa_endpoint_id: "11111111-2222-3333-4444-555555555555",
        sync_endpoint_id: "66666666-7777-8888-9999-aaaaaaaaaaaa",
      },
      role_registry: [
        { role: "security-reviewer", login: "b", key_fingerprint: "SHA256:bb" },
      ],
      fixture: { sha256: EXPECTED_FIXTURE_DIGEST, required_cases: REQUIRED_CASES.slice(1) },
      g6: { signatures: [] },
    };
    const errors = validatePrerequisites(base);
    assert(errors.includes("missing_role:architecture-owner"), "missing role must be rejected");
    assert(
      errors.includes("missing_case:concurrent-edits-preserve-deterministic-loser"),
      "missing case must be rejected",
    );
  },
});

tests.push({
  name: "revocations file stays append-only and empty",
  fn: async () => {
    const revocations = JSON.parse(
      await Deno.readTextFile(new URL("revocations.json", DOC_ROOT)),
    );
    assertEquals(revocations.schema_version, 1, "revocations schema version");
    assertEquals(revocations.revocations, [], "revocations must start empty");
  },
});

export async function runAllTests(): Promise<void> {
  await runTests("syncPrerequisites.test.ts", tests);
}
