# Development browser test inventory

The [entry source-input binding](source-binding.md) connects the original
inventory digest to the same buffer supplied by the test Vite load hook. It does
not attest transformed code, transitive dependencies or products.

The existing colocated runner and browser loader share an explicit 262-file
catalog: 250 Floorp files, two Floorp harness-compatibility files, two legacy
Mozilla adaptations and eight locked Mozilla wrappers. This is an inventory of
the current **development** suite. Every browser entry is `not-onboarded` for
packaged execution; no path-based classification establishes product
eligibility.

```sh
deno task test --list --inventory /tmp/new-browser-inventory.json
deno task test --origin floorp --inventory /tmp/new-floorp-development-plan.json
```

`--origin` filters development execution. The default remains the combined
suite. The JSON destination must be new; it is never replaced. The ordinary
browser CI saves `_dist/ci-browser-test-inventory.json` alongside its original
logs. `--list` is an informational observation, including missing materials;
execution rejects an empty selection, an unclassified selected browser file or a
missing required catalog file before checking for or starting a browser. A new
browser test must be classified explicitly in the catalog. A changed selection
remains visible against the complete discovery and catalog counts. Origin
filtering and `--near` are developer selections, not acceptance of a reduced
required product plan.

The eight generated wrappers are absent in a fresh checkout until the existing
locked collection/preparation step runs. The complete suite reports eight
missing required sources and refuses. An explicit development selection derives
its required files from the closed catalog using the same near/layer/origin
predicate, so a missing selected file cannot disappear. Missing unselected files
remain in separate observations and global counts. Existing 6/4/3-case workspace
workflows therefore do not need unrelated Mozilla resources. Keep the existing
lock, closure hashes, wrapper/task-count checks, replaced-head and
locked-but-not-loaded-support limitations. This change does not obtain or
execute the native Mozilla harness, expand upstream coverage, or qualify
Debug/optimized products.

Discovery covers the existing colocated TypeScript/JavaScript naming convention
across the repository, including tooling and unknown nonbrowser files. It does
not enumerate every Python probe, native manifest or upstream Firefox candidate.
The existing upstream quarantine is copied into a separate observation and
counted separately from colocated files. Zero quarantined colocated files does
not mean there are no quarantined upstream candidates. Future manifest-wide
discovery and feature/target migration review are still required.

Each discovered file has an SHA-256 snapshot. Browser results must report the
exact catalog ID, version, origin and execution kind; duplicates, foreign IDs,
missing metadata and a misplaced downloaded-Firefox marker fail collection. The
existing expected-result, missing-file, abort/skip and task checks remain. The
inventory starts with executed=0; it is a plan and source snapshot, not a signed
result or provider attestation. All reports keep product `UNVERIFIED` and
publication unauthorized. It cannot replace final-artifact evidence, qualified
history, approval or publisher revalidation.

Restart probes use a distinct one-shot `restart-bootstrap-isolation` purpose,
the exact existing nondiscovered sentinel, matching original file/prefs and a
fresh lease of at most five minutes. Only that explicit control may complete
with no automatic suites; its state says `qaVerdict: NOT_EXECUTED`. It is
startup isolation for the separately observed restart cases, never empty QA
acceptance. Ordinary empty execution, stale/foreign/mismatched controls and
prefs-only fallback cannot grant it. The isolated Python writer emits the
purpose marker.

Host fault tests use the real discovery, plan, reconciliation and runner entry
point. The fixed 109-case source suite is unchanged; these additional tests run
in the existing ordinary host-tool CI.
