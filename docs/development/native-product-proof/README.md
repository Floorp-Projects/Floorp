# macOS native-product proof: publication boundary

This is a **non-green evidence handoff**, not a product fix or a release gate.
It preserves the completed, focused checks from 2026-11-15 without publishing
private profiles, raw logs, binaries or unreviewed candidate patches.

## Existing reusable machinery

[PR #3138](https://github.com/Floorp-Projects/Floorp/pull/3138) already contains
`tools/qa/native-product-tests/{run.py,suites.json,test_runner.py,README.md}`.
All four files in the Mac candidate source were compared byte-for-byte with
that PR branch on 2026-11-16 and matched. They are deliberately not duplicated
here. The runner's documented P0/P1/P2 selection, provenance preflight, separate
profiles, native timeout and leak accounting remain the execution contract.
This evidence directory neither installs that runner nor changes CI.

## Verified focused outcomes

The adjacent [JSON ledger](macos-focused-2026-11-15.json) records structured
`test_end`, native process exit, unexpected shutdown statuses, completion time,
and SHA-256 identities of the local raw evidence. It is allowlisted summary
data, not copied raw logs. The original evidence is not publicly retrievable;
hashes permit a maintainer who has that evidence to compare it, but are not
independently reproducible proof by themselves.

| Check | Behavioral test_end | Native exit | Final verdict |
| --- | --- | --- | --- |
| `browser_588426.js` | PASS | 1 | FAIL: shutdown window |
| `browser_595601-restore_hidden.js` | PASS | 1 | FAIL: shutdown windows |
| Added workspace visibility roundtrip diagnostic | PASS | 1 | FAIL: shutdown window |

Each run emitted `suite_end`. None is a clean pass. The candidate was a native,
test-enabled, **unoptimized Debug** macOS product, not the installed release.
Source base commits and binary hashes are in the ledger, but the candidate
included dirty prerequisite changes. The base commits alone cannot reproduce it.
Product and object-directory XUL hashes differ; packaging is not assumed to
establish equivalence. No full-suite totals are inferred from these three cases.

The roundtrip diagnostic is evidence of what was observed, not a finished
regression test: its navigation step does not robustly await completion before
checking visibility. It needs that correction and fresh verification before
being shipped as test code. The unchanged upstream cases and diagnostic all
retain their final failure verdict here.

## Reproduction and comparison checklist

1. Start from a dedicated test product and the pinned Runtime with a native
   tests-enabled object directory. Finish product assembly and include that
   same build's testing modules. Follow #3138's runner README; do not use an
   installed daily browser or a personal profile.
2. Record source commits **and all applied deltas**, product identity, executable
   and engine hashes, module inventory and selected harness/configuration.
   A clean source checkout plus only the base commits in this ledger is not the
   historical candidate and must not be labeled an exact reproduction.
3. Use fresh profiles and logs. Keep native expected results, skip conditions,
   CSP and `MOZ_DISABLE_NONLOCAL_CONNECTIONS=1`; do not extend timeouts or suppress
   leak checking to obtain green results. Use chunking only with its documented
   loss of cross-chunk state coverage. Never rerun merely to replace a failure.
4. Run an **unpatched product comparison** with matching build configuration and
   test selection before attributing the shutdown leak to the visibility change.
   That comparison is still unverified in this handoff. This document starts no
   build or browser run and does not authorize changing existing sessions.
5. Preserve both behavioral and harness outcomes. A `test_end: PASS` followed by
   shutdown failures or a nonzero native exit stays failed. Record missing
   `suite_end`, skips and unobserved cases distinctly. Do not count assertions,
   manifest files and Marionette cases as interchangeable totals.
6. Review any publishable summary against the original raw JSONL and worker exit
   records. Retain raw evidence privately unless separately reviewed for public
   sharing. Hashes are provenance identifiers, not substitutes for review.

## Asset disposition

| Asset category | Disposition |
| --- | --- |
| Reusable native runner, suite config, unit tests and setup instructions | Already in #3138; identical Mac copies omitted |
| Focused completion/provenance evidence | Sanitized allowlisted ledger in this PR |
| Product visibility and split-panel patches | Excluded: candidate state and unresolved shutdown failures |
| Additional workspace diagnostic source | Excluded pending navigation synchronization and rerun |
| Ad hoc parallel/recovery launchers and port overrides | Excluded: local candidate topology, not a portable supported runner |
| Historical `coverage-ledger.json` and its ad hoc generator | Excluded: stale snapshot and incomplete later parallel-log reconciliation; not final coverage |
| Other dirty frontend/native fixes | Excluded from this test-evidence PR; not swept out of mixed candidate source trees |
| `.app`, build directories, archives, caches and generated modules | Excluded: generated binaries or build outputs |
| Raw console/JSONL/TBPL, profiles, modified preferences, screenshots and command/provenance dumps | Excluded: potential private paths/state and excessive raw data |

This is intentionally a Draft handoff. No fix is declared complete, no required
CI status is added, and no merge or release is performed.

## Earlier prerequisite-fix inventory (not published as code here)

The local delivery bundle also contains finished targeted work that must not be
confused with the later visibility candidates. Its patches are separate from
#3138's harness. A keyword PR search is not proof of absence; the status below
states only the overlap actually checked on 2026-11-16.

| Prerequisite | Local source/evidence | Remote overlap and publication status |
| --- | --- | --- |
| PWA non-web/destroyed-document observation guard | `browserObservation.ts`, `iconProcesser.ts`, `manifestProcesser.ts`, `ssbManager.ts`, `pwa-browser-observation.test.ts` in `delivery/floorp-proof-fixes.patch`; historical representative run has exit 0 | Related [#3049](https://github.com/Floorp-Projects/Floorp/pull/3049) changes PWA actor/page-action lifecycle and does not include `browserObservation.ts`; exact fix publication not established. Needs isolated patch comparison/review, not silent omission as already shipped. |
| Preferences observer/listener lifetime | `about-preferences.ts`, `document-lifetime.ts`, `document-lifetime.test.ts` in same bundle; historical representative run has exit 0 | No exact matching publication verified. Isolate from the mixed candidate tree and check current-main overlap before a product-fix Draft. |
| CSP and bundled icon/image assets | Modal/profile-manager/newtab/welcome HTML, icon model and build-plugin source/tests in same bundle; delivery report records final UI gate exit 0 | Not included in #3138. Modal extension-provided data/HTTPS icon compatibility remains unresolved; do not label this entire group release-ready. `pages-*/build/*.ts` here are handwritten build-plugin source, not generated outputs. |
| Native empty UTF-8 append | `delivery/character-data-empty-utf8-append.patch`; isolated pinned-source apply/reverse validation; later `empty-append-gtest.log` verifies 20 tests, 0 failures | No exact matching PR verified. Requires native rebuild, not artifact-only frontend packaging. Preserve patch plus focused tests for separate review. |

The historical `final-targeted.exit` and `p2-final.exit` are both 0, but precede
later builds; their success cannot be assigned to the current candidate. The
native gtest success was independently checked in its log. The delivery report's
broader UI claims are historical report evidence, not fresh reruns in this PR.
Publishing these product changes safely still requires applying each coherent
patch to current main, checking overlap and running its focused tests. This
inventory does not claim that work has already been done.
