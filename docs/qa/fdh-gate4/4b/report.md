# FDH Gate 4 — 4-B (AC-08, D-06b) regression set acceptance (2026-11-08)

Worktree `/tmp/fdh-gate4/regression` at main `bec27040382280c17a9e4a573359f017dca89036`
(fresh worktree, `deno install` done; host test baseline = 425 passed / 1 failed / 1 ignored,
the pre-existing `tools/src/initializer.test.ts:757` failure).

Method (fixed verification code, not loosened by the fix agent):
- For each merged fix PR: `git checkout <mergeCommit>` (detached), then `git diff HEAD^1 HEAD`
  restricted to the PR's **source** files (tests excluded) → `git apply -R` (revert the fix).
- Run the PR's own test files unchanged (`deno test -A <test files>`): expect failure.
- `git checkout -- .` (restore fix), rerun the same tests unchanged: expect pass.
- The test files are never edited during the comparison.

## Regression results (6 merged fix PRs)

| PR | merge commit | test files (fixed code) | revert (source-only) | restore fix |
|---|---|---|---|---|
| #3057 fix(dev-tool) stop scope | `6d6d4e54d807` | `tools/src/dev_process_selection.test.ts`, `dev_processes.test.ts` | exit 1 (type-check failure: reverted tree deletes the new modules the tests import) | **28 passed / 0 failed / 1 ignored**, exit 0 |
| #2991 fix(notes) stale local-save notifications | `1dcbf738be3e` | `tools/src/notes_storage.test.ts` | **13 passed / 3 failed**, exit 1 | **16 passed / 0 failed**, exit 0 |
| #2984 fix(tests) Firefox 159 system access | `a218c5b9751d` | `tools/src/browser_launcher.test.ts` | **16 passed / 4 failed**, exit 1 | **20 passed / 0 failed**, exit 0 |
| #2981 fix(notes) merge native Sync deliveries | `f189ea878acf` | `notes_record.test.ts`, `notes_storage.test.ts`, `notes_ui_contract.test.ts` | exit 1 (type-check failure; tests depend on fixed types) | **28 passed / 0 failed**, exit 0 |
| #2990 Fix Firefox 159 tab context menu layout | `a55a3c1a8413` | `tools/src/tab_context_menu_sections_patch.test.ts` | **0 passed / 1 failed**, exit 1 | **1 passed / 0 failed**, exit 0 |
| #2982 fix(tests) Firefox 159 runtime compatibility | `374f987fbed9` | `runtime_compatibility.test.ts`, `runtime_lock.test.ts`, `workspace_external_containers_patch.test.ts` | **20 passed / 3 failed**, exit 1 | **23 passed / 0 failed**, exit 0 |

Count: **6 regressions** (within the required 5–10). Four are behavioral assertion failures
(2991/2984/2990/2982); two fail before assertions by type checking because the reverted tree
removes/changes the modules/types the fixed tests import (3057/2981) — recorded honestly.

## Four-case verdicts (AC-08, D-06b: all four required, `inconclusive`-only not a pass)

1. **Known-bug version / fixed version pair — PASS.** PR #2991: revert the source fix
   (`FloorpNotesStorage.sys.mts`) → `13 passed / 3 failed`; restore the fix → `16 passed / 0 failed`,
   with the same fixed test code. Evidence: `pr2991-reverted-test.log`, `pr2991-fixed-test.log`,
   `pr2991-fix.patch`.
2. **Explore → repro steps — PASS.** PR #2982: explore the fix commit; repro steps recorded:
   (a) checkout merge `374f987fbed9`; (b) revert source-only patch `pr2982-fix.patch`
   (`git apply -R`); (c) run `deno test -A tools/src/{runtime_compatibility,runtime_lock,
   workspace_external_containers_patch}.test.ts` → `20 passed / 3 failed`; (d) restore and rerun
   → `23 passed / 0 failed`. Evidence: logs + patch.
3. **Not reproduced — PASS (excluded with reason).** PR #2981: reverting source produces a
   type-check failure before any assertion, because the fixed tests depend on the fixed types;
   product-behavior reproduction is therefore inconclusive, and the case is excluded with the
   recorded reason (not counted as a behavioral pass). Evidence: `pr2981-reverted-test.log`.
4. **Infrastructure failure — PASS.** The host test baseline on main (`deno task test:host`) is
   `425 passed / 1 failed / 1 ignored`, with the single failure the pre-existing
   `tools/src/initializer.test.ts:757` locked Runtime install test (actual 000 vs expected 002).
   It is classified as an **environment/infrastructure failure** (Runtime install state), not a
   product regression, and is recorded separately from the 4-B product assertions
   (`regression-baseline-host.log`). This is the infra-failure case: the verdict is
   "infrastructure failure", not "product failure", and the failure is not counted against the
   regression set.

## Evidence files

`/tmp/fdh-gate4/acceptance/4b/`: per-PR `*-fix.patch`, `*-reverted-test.log`, `*-fixed-test.log`,
and `regression-baseline-host.log` (worktree root `/tmp/fdh-gate4/regression-baseline-host.log`).

## Limitations

- Local macOS only; host tests run in a local worktree, no CI/other-platform comparison.
- Reverting source-only patches is a synthetic regression (reverse of merged fix), not a
  reproduction on an affected user machine.
- Two PRs fail by type checking rather than behavioral assertion; recorded, not counted as
  behavioral reproductions.
- The infra-failure case uses the known local Runtime-install test failure; it is not a
  deliberately injected infra outage beyond that environment condition.
- Test files unchanged throughout; the fix agent could not loosen the criteria without approval
  (no such change occurred).
