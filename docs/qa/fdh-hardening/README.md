# FDH hardening (post-v0.1) — acceptance record

Date: 2026-11-08. Base commit: `3dc931a16b4d` (PR #3076 merge). Worktree:
`fdh-hardening`; no Floorp repo push from the evaluation worktrees.

This record covers the P0/P1/P2 hardening items derived from the FDH v0.1
hands-on evaluation (`fdh-improve/imp1`–`imp5`, `summary.md`). It is an
acceptance record, not a Gate 3/4 qualification.

## Delivered

| Priority | Item                                                      | Files                                                                                                          |
| -------- | --------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------- |
| P0       | IMP-1 `fdh verify build` two-phase, default `all`         | `tools/src/fdh_verify.ts`, `tools/fdh.ts`, `tools/src/fdh_evidence.ts`                                         |
| P0       | IMP-3 stale Atlassian/PR records                          | Confluence 8061156 v9, 8061122 v15, 7536642 v5, 8061015 v10 (D-14), Jira KAN-104 完了; PR #3076 body 259 files |
| P1       | IMP-2 `fdh record` for fdh-external commands              | `tools/src/fdh_task.ts`, `tools/src/fdh_evidence.ts`, `tools/fdh.ts`                                           |
| P1       | IMP-5 `fdh task prepare` + relative devdir symlinks       | `tools/src/setup.ts`, `tools/src/injector.ts`, `tools/feles-build.ts`, `tools/fdh.ts`                          |
| P2       | IMP-4 D-14 review receipts + required `review-gate` check | `tools/src/fdh_review.ts`, `.github/workflows/fdh-review-gate.yml`, `tools/fdh.ts`                             |
| P2       | Locked Runtime VERSION marker uses the target platform    | `tools/src/initializer.ts` (`runtimeVersionFor`)                                                               |

## Local acceptance evidence

Task states and manifests are local-only under `_dist/fdh/` (git-ignored):

- `_dist/fdh/evidence/hardening-ac.json` — default `verify build` (before-mach
  then after-mach) plus `verify test`: manifest `state:succeeded`, assertions
  `build-exit-code`, `build-inject-exit-code`, `host-test-exit-code` all true;
  no "No build verification" and no "Source compilation" limitation.
- `_dist/fdh/evidence/hardening-aft.json` — `--phase after-mach` only: manifest
  `state:succeeded` but limitation "Source compilation was not verified; the
  recorded build assertion is after-mach XHTML injection only."
- `_dist/fdh/evidence/hardening-cli-final.json` — recorded external host-test
  failure (`fdh record --verification host-test --exit-code 1`): manifest
  `state:failed`, `external-host-test-exit-code` false, operator-attestation
  limitations present.
- `_dist/fdh/evidence/hardening-ac-browser.json` — same task after `dev up`,
  `verify browser` (connected) and `--eval ... --expect '"offset":100'`:
  manifest `state:succeeded`, `build-exit-code`/`build-inject-exit-code`/
  `host-test-exit-code`/`browser-connected`/`browser-eval-contains` true. The
  earlier failed `browser-eval-contains` (a wrong pref getter) remains visible
  in `assertions[]` while the latest-per-name outcome is a pass.
- `_dist/fdh/reviews/hardening-cli-3dc931a16b4d.json` — review receipt with
  marker comment body; `fdh review record` writes it locally and the agent posts
  it to the PR.

Checks exercised during this run:

1. Broken `browser-features/pages-settings/src/components/app-sidebar.tsx`
   (syntax error) with default `verify build`: exit 1, `build-exit-code`
   `passed:false`, and **no** `build-inject-exit-code` operation (the injector
   did not run). Source restored afterwards.
2. Fixed source, runtime present: default `verify build` passed both phases.
3. `fdh task prepare --runtime-source <registry>/_dist/bin` copied 674 MiB in
   ~22 s, relinked `noraneko-devdir` to **relative** targets, recorded
   `setup.guardEnv.FLOORP_SKIP_PATCHER=1`, and a second run was an
   `already`/relink no-op. The relink fix matters: the copied tree originally
   kept absolute `/.../fdh-eval-b` links, which would break fonts and modules.
   After the off-by-one fix, `realPathSync` on a mount link resolves inside the
   worktree; a same-root test catches a regression that the shallow temp-root
   test missed.
4. `fdh record` refused out-of-worktree cwd/observation files and exit-code
   mismatch in unit tests; changing the observation file after attestation made
   `fdh evidence` fail with `schema-violation /inputFidelity/inputs/1`.
5. `fdh review` unit tests: commit mismatch, `request_changes`, `blocking=true`
   and fewer than two distinct reviewers all fail the gate.
6. `deno task test:host`: **480 passed, 0 failed, 1 ignored** (15 s). The
   previously failing `locked Runtime install ... retains recovery state` test
   now expects `002` for the windows target and passes.
7. `fdh task prepare` then `fdh dev up` started a browser in **53.7 s** (28.9 s
   on the re-run after the symlink fix) with
   `Browser ready! Marionette on port 5280` (no manual `deno install`, `cp`,
   `rm`, or `FLOORP_SKIP_PATCHER` export). The gate workflow now also listens to
   `issue_comment`; for this PR (which introduces the workflow), post the two
   receipts and rerun the failed `review-gate` check once. `fdh verify browser`
   recorded a connection and an eval contains-check of the recorded offset 100;
   `fdh dev down` stopped 32 processes. After the symlink fix, the settings
   content page loaded at `http://localhost:5283/#/overview/home` with 394
   characters of body text; the browser console was busy with BiDe sessions, so
   the font/404 console check was not recorded.
8. The Patcher guard was missing from `main`: `fdh task prepare` recorded
   `FLOORP_SKIP_PATCHER=1` but `tools/feles-build.ts` ignored it, so dev aborted
   on a packaged Debug Runtime. Added the guard at the three
   `Patcher.run("apply")` sites (dev/stage/test), not at the explicit
   `misc patch` command.

## Limits

- Browser confirmation is recorded only for the `hardening-ac` task; the other
  acceptance manifests carry the `Browser confirmation was not verified`
  limitation.
- This is a local macOS run; `matchesTarget` is false for the qualification
  environment.
- The review gate needs at least two distinct reviewer identities at the PR
  head; the PR must carry both marker comments before `review-gate` can pass.
- The external-record exit code and operator are self-declared; the manifest
  re-hashes the observation file but cannot prove the execution itself.
- The runtime registry used here was another local worktree
  (`fdh-eval-b/_dist/bin`); a fresh machine still needs a registry path or a
  Debug Runtime install.
