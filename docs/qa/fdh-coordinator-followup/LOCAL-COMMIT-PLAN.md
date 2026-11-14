# Local-only commit plan (operator authorized option 1)

The user explicitly selected local split commits. No push, PR, merge, workflow dispatch, publication, repository configuration or Atlassian Artifacts upload is authorized by that selection. Existing Git hooks/signing configuration is preserved.

Baseline: 9e6b24e5fd561250ab04a011b70ee0b1280b1adc. Branch: fdh-coordinator-followup. The original checkout's initial patch changes and other untracked files are excluded. Generated _dist files, downloaded ZIPs, node_modules and private runtime/token material are never staged.

## Six logical groups
| Group | Contents | Dependencies |
| --- | --- | --- |
| 01-dev-os-isolation | Dev port/profile token scope, serialized adapter/reset, TS API auth client/wrapper and synthetic tests | Keep runtime adapter+shared definitions+profile opt-in+client/options together; post-build adapter integration remains separate from canonical host tests |
| 05-readonly-qa-chain | Pure consistency/archive/composition, bounded gh transport, collector, strict lazy CLI and tests/integrations | Complete helper→collector→CLI chain in one commit; authoritative source eligibility helper remains baseline/unchanged |
| 02-macos-supervisor | Python macOS supervisor, ownership/error repairs, contract/native test definitions and hosted CI | Source and kind-signature regression tests together; native macOS still unverified |
| 03-trusted-receipts | Trusted receipt-only workflow, helper/runner/types/tests and executable harness | Reuses existing baseline validator; includes the workflow-contract test (do not omit it) |
| 04-alpha-environment | Alpha environment reference + static publication-contract tests | Does not configure required reviewers or activate final-package QA |
| 06-docs-and-skills | Handoff/design/current verification documents, this plan, source inventory and project skill | Last so documentation references all prepared features |

Groups 01–05 were independently import-audited and can be staged by exact file list; suggested linear order is 01,05,02,03,04,06. Prefix tools/integration is NOT a single group; use its individual mapped files. No blanket git add -A is used. The local change inventory lists every mapped file and raw SHA-256; it excludes its own output to avoid recursive hashing. Raw working-file hashes are not normalized Git blob hashes.

## Local code commits created

| Group | Commit |
| --- | --- |
| 01-dev-os-isolation | f1c1e460d00799c4a64db41db15f8af8026fe6c3 |
| 05-readonly-qa-chain | 9ab7c125b5f5443ffe80fa98fc36fb369ef3e55f |
| 02-macos-supervisor | 2c250ad6aabb7b078c15e1c94778fd11643e877d |
| 03-trusted-receipts | f4bcfd4477e8318830c02fee9fac1814c4c8b8b5 |
| 04-alpha-environment | f380b3ae6a0f5962dd356b8e77f994195436a8fb |

The final docs/inventory commit follows these five. Its own ID is available in local Git history, not recursively embedded in its content. The inventory compares the complete change set against the original baseline, so it remains verifiable after committing; modified/added status is relative to that baseline, not staging state.

## Execution boundaries
Each code group is checked against the captured bytes, staged only by explicit paths, its staged path set is compared with that group, and whitespace is checked before commit. Default Git hooks/signing are not disabled. A failure stops the sequence for investigation. Documentation is finalized and its inventory regenerated before the final docs commit. Post-commit canonical host tests/source build and a clean worktree check remain required.

This is local history preparation, not release qualification. All existing native/runtime/approval/OAuth/Windows-token and live pending-positive limits remain in the current collector/checkpoint records. Commit IDs are recorded by local Git history and the final handoff; no remote receipt is fabricated from local file reviews.
