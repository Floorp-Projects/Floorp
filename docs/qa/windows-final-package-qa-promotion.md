# Windows final-package QA promotion (KAN-130)

The `windows/x86_64` job in
[Validate final Floorp package regressions](../../.github/workflows/validate-final-package-qa.yml)
runs on GitHub-hosted `windows-2025`. Stable and standalone QA tolerate a
Windows job failure in shadow mode. Windows-only Alpha already requires the
Windows result.

## Evidence required for promotion

- Require **five consecutive first-attempt passes on five distinct signed Stable
  installer payloads** across release candidate runs on `main`. Use the same-run
  `final-package-qa` call with the signed bundle and exact installer IDs.
  Deduplicate across runs by the SHA-512 of
  `floorp-windows-x86_64.installer.exe`. Count each hash once per streak. A
  passing run with a repeated hash leaves the streak count unchanged, even if
  the run, bundle, or installer artifact has a new ID. A Windows failure, skip,
  timeout, cancellation, or missing evidence resets the streak. Restart the
  streak after changes to the Windows runner image label, probe behavior, or
  package authentication. Check the Windows job steps and reports for a pass.
  Log and triage failed attempts even when the overall workflow is green.
- For each candidate, record the package run and attempt, Floorp commit, Windows
  job URL, runner image version, duration, and the
  `final-package-qa-windows-x86_64-<run_id>-attempt-<attempt>` artifact. Check
  `input.json` for the signed bundle ID and digest. Compare the installer
  artifact ID and digest and Runtime BuildID with `native-verification.json`.
  From the matched bundle's `release-manifest-set-v2.json`, record
  `release_files.windows.sha512` and verify it against the SHA-512 of the signed
  `.exe` extracted from that installer artifact. Require `status: passed` and
  successful cleanup in both probe reports at `web-panel-ua/run-*/report.json`
  and `fluent-flasco/run-*/report.json`. Preserve the evidence before the 14-day
  Actions artifact retention expires and link it in the promotion PR. Unsigned
  validation, Alpha runs, supervisor-only tests, and standalone dispatches are
  outside this Stable streak.

## Switch to required

1. In the reusable QA workflow, remove the job-level
   `continue-on-error: ${{ matrix.platform == 'windows' && !inputs.alpha }}`
   from `jobs.final-package` and update the matching assertion in
   [the publish workflow host tests](../../tools/src/fdh_publish_workflow.test.ts).
   This makes Windows required for both Stable and standalone QA. Keep the
   `alpha` input for source workflow authentication.
2. In [the Stable publisher](../../.github/workflows/publish_release.yml),
   `prepare_release` and `release_check` already depend on `final-package-qa`.
   During a controlled release rehearsal with approval withheld, check that a
   Windows failure blocks both jobs and that a green matrix allows the run to
   reach protected approval. The release gate is enforced by these job
   dependencies.
3. If FDH discovery or collection is used to assess the promoted Stable
   candidates, set the trusted caller's `windowsPolicy` to `require-success`
   (`--windows-policy require-success` for the CLI). FDH evidence assessment is
   separate from the publisher dependency.
