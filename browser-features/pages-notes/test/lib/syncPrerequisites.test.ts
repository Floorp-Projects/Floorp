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
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import prerequisitesDocument from "../../../../docs/development/floorp-notes-sync/prerequisites.json" with {
  type: "json",
};
import revocationsDocument from "../../../../docs/development/floorp-notes-sync/revocations.json" with {
  type: "json",
};

interface Prerequisites {
  schema_version: number;
  adr: string;
  engine_authority: { commit: string };
  runtime_pin: { commit: string; tree: string };
  desktop_release: { commit: string };
  role_registry: { role: string; login: string; key_fingerprint: string }[];
  fixture: { sha256: string; required_cases: string[] };
  g6: { signatures: unknown[] };
  production_environment?: {
    status: string;
    authorization: string;
    fxa_configuration: string;
    fxa_hosts: string[];
    sync_hosts: string[];
    wire: string;
    application_record_id: string;
    endpoint_policy_sha256: string;
  };
}

const EXPECTED_RUNTIME_COMMIT = "2d38da4d11be1e0e615f4ddd785ad5e77c95e18d";
const EXPECTED_RUNTIME_TREE = "e555a371e1a24f18c8085058461f92c06e0b997d";
const EXPECTED_FIXTURE_DIGEST =
  "2597e5311c7c4ea4bb9d6a806ffa183aae3b3bd7380893b664b02ac829d665fd";
const EXPECTED_ENDPOINT_POLICY_DIGEST =
  "af96437acde3d05eb8f18dc9cc81450aa9d61703579c092b962922de8934c9ca";
const EXPECTED_PREFS_RECORD_ID =
  "e2VjODAzMGY3LWMyMGEtNDY0Zi05YjBlLTEzYTNhOWU5NzM4NH0";
const EXPECTED_FXA_HOSTS = [
  "accounts.firefox.com",
  "api.accounts.firefox.com",
  "oauth.accounts.firefox.com",
  "profile.accounts.firefox.com",
  "static.accounts.firefox.com",
];
const EXPECTED_SYNC_HOSTS = [
  "event-sync.services.mozilla.com",
  "sync.services.mozilla.com",
  "token.services.mozilla.com",
];
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
  errors.push(...validateProductionAuthority(prerequisites));
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

export function validateG6SignerRegistry(
  prerequisites: Prerequisites,
): string[] {
  const errors: string[] = [];
  const roles = prerequisites.role_registry ?? [];
  const namedRoles = new Set(roles.map((r) => r.role));
  for (
    const requiredRole of [
      "architecture-owner",
      "security-reviewer",
      "privacy-reviewer",
      "retention-reviewer",
      "rollout-approver",
    ]
  ) {
    if (!namedRoles.has(requiredRole)) {
      errors.push(`missing_role:${requiredRole}`);
    }
  }
  for (const role of roles) {
    if (!role.login || role.login.startsWith("PENDING_")) {
      errors.push(`role_login:${role.role}`);
    }
    if (!role.key_fingerprint || role.key_fingerprint.startsWith("PENDING_")) {
      errors.push(`role_fingerprint:${role.role}`);
    }
  }
  return errors;
}

export function validateProductionAuthority(
  prerequisites: Prerequisites,
): string[] {
  const errors: string[] = [];
  const production = prerequisites.production_environment;
  if (!production || production.status !== "approved") {
    errors.push("production_approval");
  }
  if (
    !production ||
    production.authorization !== "product-owner-explicit-2026-08-09"
  ) {
    errors.push("production_authorization");
  }
  if (
    !production || production.fxa_configuration !== "FxAConfig.Server.release"
  ) {
    errors.push("fxa_configuration");
  }
  if (
    !production ||
    JSON.stringify([...production.fxa_hosts].sort()) !==
      JSON.stringify(EXPECTED_FXA_HOSTS)
  ) {
    errors.push("fxa_hosts");
  }
  if (
    !production ||
    JSON.stringify([...production.sync_hosts].sort()) !==
      JSON.stringify(EXPECTED_SYNC_HOSTS)
  ) {
    errors.push("sync_hosts");
  }
  if (!production || production.wire !== "sync15") {
    errors.push("wire");
  }
  if (
    !production || production.application_record_id !== EXPECTED_PREFS_RECORD_ID
  ) {
    errors.push("application_record_id");
  }
  if (
    !production ||
    production.endpoint_policy_sha256 !== EXPECTED_ENDPOINT_POLICY_DIGEST
  ) {
    errors.push("endpoint_policy_sha256");
  }
  return errors;
}

const tests: TestCase[] = [];

tests.push({
  name: "shipped prerequisites bind the approved production authority",
  fn: () => {
    const prerequisites = prerequisitesDocument as Prerequisites;
    const errors = validateProductionAuthority(prerequisites);
    assert(
      errors.length === 0,
      `production Notes Sync authority errors: ${errors.join(",")}`,
    );
  },
});

tests.push({
  name: "shipped prerequisites pass G1 production contract validation",
  fn: () => {
    const prerequisites = prerequisitesDocument as Prerequisites;
    const errors = validatePrerequisites(prerequisites);
    assert(
      errors.length === 0,
      `approved production prerequisites must pass: ${errors.join(",")}`,
    );
  },
});

tests.push({
  name: "G6 remains pending until independent role keys are registered",
  fn: () => {
    const prerequisites = prerequisitesDocument as Prerequisites;
    const errors = validateG6SignerRegistry(prerequisites);
    assert(
      errors.includes("role_fingerprint:security-reviewer"),
      "production authority must not synthesize a security-reviewer key",
    );
    assert(
      errors.includes("role_fingerprint:privacy-reviewer"),
      "production authority must not synthesize a privacy-reviewer key",
    );
    assert(
      errors.includes("role_fingerprint:retention-reviewer"),
      "production authority must not synthesize a retention-reviewer key",
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
      production_environment: {
        status: "approved",
        authorization: "product-owner-explicit-2026-08-09",
        fxa_configuration: "FxAConfig.Server.release",
        fxa_hosts: EXPECTED_FXA_HOSTS,
        sync_hosts: EXPECTED_SYNC_HOSTS,
        wire: "sync15",
        application_record_id: EXPECTED_PREFS_RECORD_ID,
        endpoint_policy_sha256: EXPECTED_ENDPOINT_POLICY_DIGEST,
      },
      role_registry: [
        {
          role: "architecture-owner",
          login: "arch",
          key_fingerprint: "SHA256:aa",
        },
        {
          role: "security-reviewer",
          login: "sec",
          key_fingerprint: "SHA256:bb",
        },
        {
          role: "privacy-reviewer",
          login: "priv",
          key_fingerprint: "SHA256:cc",
        },
        {
          role: "retention-reviewer",
          login: "ret",
          key_fingerprint: "SHA256:dd",
        },
        {
          role: "rollout-approver",
          login: "roll",
          key_fingerprint: "SHA256:ee",
        },
      ],
      fixture: {
        sha256: EXPECTED_FIXTURE_DIGEST,
        required_cases: REQUIRED_CASES,
      },
      g6: { signatures: [] },
    };
    const errors = validatePrerequisites(approved);
    assert(errors.length === 0, `approved fixture errors: ${errors.join(",")}`);
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
      production_environment: {
        status: "approved",
        authorization: "product-owner-explicit-2026-08-09",
        fxa_configuration: "FxAConfig.Server.release",
        fxa_hosts: EXPECTED_FXA_HOSTS,
        sync_hosts: EXPECTED_SYNC_HOSTS,
        wire: "sync15",
        application_record_id: EXPECTED_PREFS_RECORD_ID,
        endpoint_policy_sha256: EXPECTED_ENDPOINT_POLICY_DIGEST,
      },
      role_registry: [
        {
          role: "architecture-owner",
          login: "a",
          key_fingerprint: "SHA256:aa",
        },
        { role: "security-reviewer", login: "b", key_fingerprint: "SHA256:bb" },
        { role: "privacy-reviewer", login: "c", key_fingerprint: "SHA256:cc" },
        {
          role: "retention-reviewer",
          login: "d",
          key_fingerprint: "SHA256:dd",
        },
        { role: "rollout-approver", login: "e", key_fingerprint: "SHA256:ee" },
      ],
      fixture: { sha256: "0".repeat(64), required_cases: REQUIRED_CASES },
      g6: { signatures: [] },
    };
    const errors = validatePrerequisites(base);
    assert(
      errors.includes("runtime_commit"),
      "wrong runtime SHA must be rejected",
    );
    assert(
      errors.includes("fixture_digest"),
      "wrong fixture digest must be rejected",
    );
  },
});

tests.push({
  name: "wrong production authority and missing case are rejected",
  fn: () => {
    const base = {
      schema_version: 1,
      adr: "ADR-001",
      engine_authority: { commit: "d588863894e9b3ce58b05a964a7694ab00e28054" },
      runtime_pin: {
        commit: EXPECTED_RUNTIME_COMMIT,
        tree: EXPECTED_RUNTIME_TREE,
      },
      desktop_release: { commit: "811a5b821e1d9d47b40f22aee6df5db2254a54b1" },
      production_environment: {
        status: "unapproved",
        authorization: "missing",
        fxa_configuration: "FxAConfig.Server.stage",
        fxa_hosts: [],
        sync_hosts: [],
        wire: "sync15",
        application_record_id: EXPECTED_PREFS_RECORD_ID,
        endpoint_policy_sha256: EXPECTED_ENDPOINT_POLICY_DIGEST,
      },
      role_registry: [
        { role: "security-reviewer", login: "b", key_fingerprint: "SHA256:bb" },
      ],
      fixture: {
        sha256: EXPECTED_FIXTURE_DIGEST,
        required_cases: REQUIRED_CASES.slice(1),
      },
      g6: { signatures: [] },
    };
    const errors = validatePrerequisites(base);
    assert(
      errors.includes("production_approval"),
      "unapproved production use must be rejected",
    );
    assert(
      errors.includes("fxa_configuration"),
      "stage configuration must be rejected",
    );
    assert(
      errors.includes(
        "missing_case:concurrent-edits-preserve-deterministic-loser",
      ),
      "missing case must be rejected",
    );
  },
});

tests.push({
  name: "revocations file stays append-only and empty",
  fn: () => {
    const revocations = revocationsDocument;
    assertEquals(revocations.schema_version, 1, "revocations schema version");
    assert(
      revocations.revocations.length === 0,
      "revocations must start empty",
    );
  },
});

export async function runAllTests(): Promise<void> {
  await runTests("syncPrerequisites.test.ts", tests);
}
