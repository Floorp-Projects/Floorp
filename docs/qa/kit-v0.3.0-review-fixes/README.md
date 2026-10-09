# QA kit v0.3.0 review fixes

This directory retains an immutable user-provided input and a separately
identified repair of three reproduced kit defects. It contains documentation,
archives and review evidence. It does not install the kit into Floorp or change
the existing source job, browser runner, workflow, signing or publication gate.

The original input is [`docs/Floorp_QA_Implementation_Kit_v0.3.0.zip`](../../Floorp_QA_Implementation_Kit_v0.3.0.zip)
from [commit 87311566a225936fbe7948db142bce3f1d3379d8](https://github.com/Floorp-Projects/Floorp/commit/87311566a225936fbe7948db142bce3f1d3379d8),
Git blob `e688e2725ce502f36c600cfec88060e7954b30e2`, 150471 bytes,
SHA256 `a4af60380399050f82465864734391227ab34024d26bdf16c31559e2824a8d50`.
The source commit is an input reference; this documentation branch is based on
`main@013bb6c9ed19250592898ec8fbc8d7d24aa847be`.

The [repaired archive](Floorp_QA_Implementation_Kit_v0.3.0_review-fixes.1.zip)
is **review-fixes.1**, 158581 bytes,
SHA256 `60ad9a63c9fa7e328cc2da41187c6b725a64ee6144aaac8f66c2ebc3260710f2`.
Its embedded design version remains 0.3.0. `DESIGN.md` and the original 14
documents are byte-for-byte unchanged. `REVIEW_FIXES.md` and
`validation/review-fixes.json` distinguish current validation and unresolved
requirements from the original historical reports.

Review the [plaintext patch](kit-fixes.patch) alongside the
[archive receipt](archive-validation.json). The patch applies to an isolated
copy of the original extracted kit and reconstructs the repaired payload
exactly. The repaired ZIP has 60 regular entries and a regenerated 59-file
manifest; CRC, path/type/size checks and exact patch roundtrip passed. Generated
tool caches are excluded.

| Finding | Reproduction and repair |
| --- | --- |
| KIT-P1-01 | A single attempt `2^53-1` caused `MemoryError`. Check the actual history without a maximum-proportional allocation; bound integer attempts to 1–100 and total records to cases×100 in the implementation and schema. A 256-MiB child returns BLOCK for the huge input; complete history 100 passes and 101 blocks. |
| KIT-P1-02 | Zero assertions, retry WARNING, child exit 1 and an unknown action each passed. Require exact suite selection and positive assertions; reject retry diagnostics, unknown actions and untyped/nonzero child exits. Bound raw mozlog bytes, line size and event count. |
| KIT-P1-03 | One stdin byte without newline/EOF blocked beyond the campaign deadline. Read nonblocking bytes against one absolute deadline, bound partial frames, retain pipelined frames, and restore the original fd flag during cleanup. |

The related PR84-P2-01 repair is submitted separately in
[Floorp-Runtime #84](https://github.com/Floorp-Projects/Floorp-Runtime/pull/84):
require a present integer `process_exit.exitcode` equal to zero. The follow-up
changes only the parser condition and one regression method covering eight
invalid values and valid integer zero.

## Validation and first failures

All counts below are local host checks, with zero skips on Linux x86_64 and
Python 3.12.14. These local checks launched no native browser or Mozilla harness.

| Check | Result |
| --- | --- |
| Original kit methods | 114 PASS; no test method removed or weakened |
| Repaired kit, including regression methods | 130 PASS = 114 original + 16 new |
| Fresh safe expansion of the final repaired ZIP | 130 PASS |
| Related Runtime contracts | 162 PASS = 161 previous + 1 typed-exit method |
| Existing Floorp shadow contract tests | 16 PASS |
| Existing Floorp fixed source plan at PR2889 head `718e688fbd7ad88a776534e7778ad8e4a7f67364` | 106 PASS = 60 Deno + 28 Node + 18 Python |
| Runtime Ruff check/format and targeted `mach lint --linter ruff` | PASS |
| Full Runtime modified-file `mach lint` | BLOCKED: sparse checkout lacks `third_party/node/pnpm-lock.yaml` during linter registration |

[Retained evidence](evidence/) includes the initial pre-fix kit run
(13 methods, 16 failing cases/subtests) and Runtime typed-exit run
(1 method, 3 failing subtests). Three additional kit boundary methods were
added after the initial reproductions. The initial regressions were run before
implementation changes.

Initial whole-suite failures are retained too. The first two Runtime runs had
four failures/errors because an outer test wrapper's inherited hard AS/FSIZE
limits prevented the unchanged `run_owned` fixture from installing its child
limits. Small diagnostics record the exact `prlimit` errors; correcting that
wrapper allowed all 162 tests to run. The first source-plan run retained a FAIL
when that wrapper's virtual address-space limit prevented Deno's V8 cage
reservation. The corrected source run used the worker's existing 16-GiB
physical-memory cgroup and recorded all 106 cases. These are distinct named
runs; a later PASS does not overwrite or relabel an earlier FAIL.

Raw logs and unified diffs retain their original whitespace. The local
`.gitattributes` exempts only `*.log` and `*.patch` from whitespace diagnostics;
implementation files and documentation retain normal checks. Absolute
`/workspace` references in snapshots describe the originating review workspace;
the copied files are indexed by relative path and SHA256 in `evidence-index.json`.

Two independent reviewers inspected the exact repaired code and exercised
separate fixtures. Their reports and code hashes are included in `evidence/`.
Resource-boundary checks include attempt 100/101, a real 4-MiB mozlog line and
its next byte, a 65,536-byte Agent frame and its next byte, slow partial input,
first FAIL/SKIP/history gaps, EOF, pipelining and restoration of an initially
nonblocking descriptor.

To rerun the repaired kit after safely expanding it into a **new isolated
directory**:

```sh
cd Floorp_QA_Implementation_Kit_v0.3.0
python3 -B -m unittest discover -s tools/qa/tests -v
```

POSIX-only subprocess regressions are skipped on other systems; those skips
are not native success. The attempt ceiling is a local resource limit, not
authorization to retry a required test.

The initial Draft creation also triggered the repository's existing integration
workflow [37933791186, attempt 1](https://github.com/Floorp-Projects/Floorp/actions/runs/37933791186)
at head `2d2be1eb70e89931c064de1ce23615b5f02f7a0c`. Its host smoke completed,
the ARM/Windows/Mac artifact intake jobs completed, and its Linux combined
browser suite started automatically. The pre-push workflow inspection missed
that automatic native continuation. This owned run was cancelled to preserve
the current native No-Go; REST confirms `completed/cancelled`. The before/after
records are retained in `evidence/`. That partial CI run issues no qualification
and supplies no accepted browser test count. No private Runtime Debug build
workflow was dispatched. Further PR updates can trigger this existing workflow
again; native continuation requires a separately authorized phase.

## Remaining qualification work

These four defects are repaired within their approved scope. The source-cohort
specification and its authenticated evidence binding remain incomplete: equal
U alone cannot authorize reuse across U/R/P, transforms, browser tree B,
build/target/run/attempt, diagnostics D/F, producer/controller and first-failure
history. The unchanged original evidence-contract document does not complete
that revision.

Agent startup, action and shutdown still use their existing independent
timeouts; fixing stdin does not establish a whole-browser hard deadline or
complete process/memory/disk isolation. Dedicated secret-free workers, an
external supervisor and actual native canaries must be provisioned and proven.
Windows Agent remains unimplemented. All OS/native targets, Linux ARM
production-opt, both Mac universal native executions, transformed installer/
MAR/DEB/Portable/stub bytes and final signed artifact QA remain unverified.

The existing order stays `bundle → final QA → manual approval → fresh evidence
revalidation → publish`. `publicationAuthorized=false`; native and product
qualification remain `UNVERIFIED`. This review did not provision Blacksmith,
create credentials, dispatch native workflows, merge, sign, notarize or publish.
