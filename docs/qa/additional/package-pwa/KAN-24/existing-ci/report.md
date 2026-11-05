# Existing PR2950 browser CI evidence

Canonical PR head `69d306b11c9e52da9e6270870740d49b1d561659` has successful existing browser CI [39378926878](https://github.com/Floorp-Projects/Floorp/actions/runs/39378926878), attempt1, 2026-10-20, pull_request event from Walkmana-25/Floorp. Workflow `.github/workflows/colocated_runner_test.yml`. Read-only API metadata is saved in `api-metadata.json`; no dispatch/retry/source changes were performed.

All five jobs succeeded. Actual combined browser execution and evidence upload succeeded on `Native Runtime / ubuntu-24.04` job118483530359. The ARM/macOS/Windows jobs succeeded at their applicable validation steps but skipped the combined browser suite; they are not additional browser execution platforms.

Artifact `test-logs-ubuntu-24.04` ID12244273619 was available (API expired=false, expires2027-01-18T06:11:02Z), downloaded and SHA256 authenticated: `d542e4c668ef814f2bc0fb634d8ccc58ffaa611215ecca8f0b5ee6aa64f842fe`. `browser-evidence.zip` retains the original bytes; `evidence/` holds path-checked extraction.

Authenticated `_dist/ci-combined-browser-test.log` proves actual completion, rather than discovery alone:

- Lines354–355: `tabbar-style-sync.test.ts`, runAllTests success,9ms.
- Lines360–361: `vertical-tabs-observer.test.ts`, runAllTests success,469ms.
- Lines836–837: settings integration `settings.test.ts`, runAllTests success,26360ms.
- Line864: 266 browser test modules passed,0 failed. Line865 separately reports8 downloaded Firefox files/16 tasks passed.

The two new helper/observer source modules declare3 and5 cases, respectively; the settings diff adds5 named cases. These are source declaration counts, not independently logged per-case execution counts. Exact case names/module completion lines are in `target-test-execution.json`.

This old CI ran locked Runtime158.0.1/BuildID20261014104253, explicitly reported in `_dist/ci-feles-build-test.log:3`; PR source lock points to daily1155 Runtime source `7dd619742b25567e479eeda97ae071d10542ad0b`. It does not establish behavior on today's native159 product. Keep it separate from the current latest-main+PR2950 scoped generated-resource candidate UI tests and the four-case proposal API probe. A passing historical module suite does not settle the currently reproduced initial-sync/stale-form gaps.
