# ADR-001: Floorp Notes cross-client sync contract

- Status: approved (architecture and ownership level)
- Date: 2026-08-08
- Milestone: Notes Sync 0.3.0 staging (production remains disabled)

## Context

Floorp Notes exist on Desktop (Firefox-style `floorp.browser.note.memos`
preference) and iOS (local-only archive). Both clients must eventually share
the same Notes data through the FxA/Application Services `sync15` preference
sync path without ever silently losing user content. The iOS implementation
already ships a deterministic merge and a machine-readable fixture
(`sync-fixtures/floorp-notes/floorp-notes-merge-v1.json`, digest
`2597e5311c7c4ea4bb9d6a806ffa183aae3b3bd7380893b664b02ac829d665fd`). The
Desktop implementation previously used random conflict IDs and non-commutative
winner selection.

## Decision

1. **Engine authority**: the merge engine contract is defined by the
   Application Services `floorp-prefs-sync` component (engine authority from
   AS source) and pinned by the shared fixture. iOS and Desktop implement the
   same deterministic semantics.
2. **Deterministic winner**: for concurrent edits the winner is the note that
   does not precede the other; `precedes` compares `updatedAt` ascending and
   breaks ties bytewise over canonical payload bytes
   (u64be-length-prefixed id/title/content followed by big-endian
   createdAt/updatedAt).
3. **Canonical conflict copies**: losing versions are preserved as
   `floorp-sync-conflict-<sha256>` notes with probe-based collision handling,
   never random UUIDs.
4. **Wire format**: the desktop parallel-array payload (`ids`, `titles`,
   `contents`, `createdAts`, `updatedAts`) is the interchange format; iOS
   keeps it isolated in its desktop adapter and Desktop keeps it as its
   persisted preference.
5. **Bindings**: `docs/development/floorp-notes-sync/prerequisites.json` pins
   the approved non-production FxA/Sync staging environment IDs, the role
   registry, the fixture digest, the required case set, and the
   retention/reset/rollout/migration policies. `allowed-signers` pins the
   SSH trust anchors for G6 approvals; `revocations.json` is the append-only
   revocation registry.
6. **No production enablement**: this ADR and its staging bindings never
   enable a production Notes Sync path. Production go-live is a separate,
   later authorization after all six release gates pass.

## Privacy and security

- Notes content stays end-to-end encrypted by FxA/Sync encryption in
  non-production staging only; the staging environment is account-isolated
  and never holds production data.
- No OAuth tokens or raw Sync keys reach Swift or the notes UI; the engine
  remains opaque manager state.
- Retention: staging accounts are revoked after the matrix run; proxy traces
  contain host/URL metadata only.

## Migration and rollout

- The shared fixture drives both clients' tests; Desktop's
  `mergeNotesThreeWay` now reproduces iOS winners, conflict copies, and
  ordering exactly (verified case-by-case in
  `browser-features/pages-notes/test/lib/mergeFixture.test.ts`).
- Rollout of the corrected Desktop merge is behind the sync staging flow;
  local-only usage is unaffected.

## Consequences

- Desktop and iOS derive byte-identical merge results, enabling cross-client
  staging verification and a single G1-G6 evidence record.
- Any change to the fixture or the merge contract requires a revised ADR and
  re-validation of the allowed-signers trust anchors before G6 approvals.
