# QA309 documentation and public-evidence independent review — candidate 01

Read-only review of the pending documentation in `Floorp-kit-fixes`, whose HEAD is `d9e03b8c4ccf84a52db8a7532fdd8e3fd1157f89`. The public packaging program was neither imported nor executed. No source/document changes, archive creation, network, native execution, CI dispatch, commit or push occurred. Only separate review evidence was written.

| Input at review | SHA256 | Bytes |
| --- | --- | --- |
| `FOLLOWUP_QA309.md` | `c7d4e873de236f312e7048ccbe0df3f0e88250a9e111f385457c10f8f1628ef8` | 9815 |
| `README.md` | `0b27d89c5e4f4a8f96c89552e1d4b26f0dfd3d1caa5157e7ba3a13ab97300155` | 8563 |
| `fixtures/package_public_evidence.py` | `672080da3d558f78735ee3c1837804e78dd04ae26a2f613c9d618614457c5b5c` | 6176 |

## Concrete issue

**P2-DOC-01 — desired QA3 publication ordering is described as an existing production gate.** `FOLLOWUP_QA309.md` lines 139–140 says “The production gate remains” followed by `bundle -> final QA -> manual approval -> fresh evidence revalidation -> publish`. `README.md` lines 134–135 similarly calls this the existing order. This can imply that final QA and fresh QA3 evidence revalidation already control the first public mutation.

At the reviewed Floorp code head `7747cf0e711937e056a34594d8d39fcc8dfc54f0`, `package_and_publish_release.yml` lines 526/531 depends on bundle assembly and calls the existing publisher; `validate-final-package-qa.yml` is a separate dispatch entry. `publish_release.yml` creates a draft at lines 355–370, but `release_check` lines 393–434 explicitly requires that the draft has **already** been published manually. This review does not identify a new publisher implementation defect or authorize changing it. The QA3 connection remains a known residual requirement.

Recommendation: describe the sequence as the intended qualification/publication order, state that this follow-up preserves existing release gates, and explicitly leave the QA3 required publication connection unimplemented. Acceptance is an unambiguous distinction between preserved current gates and the future QA3 chain. No new P0/P1 was found in this document/packaging scope.

## Historical immutability

The original ZIP, repaired ZIP, readable patch, archive receipt and original evidence index all matched the bytes at `d9e03b8c`. All **74** indexed historical evidence files match their stored size/SHA256. `DESIGN.md` and all **14** original kit documents match byte-for-byte between the original and repaired archives.

| Historical artifact | Verified bytes / SHA256 |
| --- | --- |
| Original ZIP, 57 members | 150471 / `a4af60380399050f82465864734391227ab34024d26bdf16c31559e2824a8d50` |
| Repair ZIP, 60 members | 158581 / `60ad9a63c9fa7e328cc2da41187c6b725a64ee6144aaac8f66c2ebc3260710f2` |
| QA308 review | 24502 / `26b8b073b2cdf3793b57f858634ecb150b6c4150c033b4f514ff2170a0bcc84f` |
| QA308 evidence ZIP, 427 members | 2238033 / `60b5386d5363edafc4fac738e32c7dd59336db2d23fffb2ba8940a81f65c91d2` |

The last two artifacts are pending additions to this documentation worktree, not files tracked at `d9e03b8c`; their measured hashes match the explicitly frozen historical values. This inspection read archive metadata and selected design-document bytes without extracting or executing their contents. It did not repeat CRC/patch reconstruction or kit unit validation; those prior receipts remain preserved.

## Counts, types and CI scope

The follow-up distinguishes Floorp's **22 contract methods + 106 source cases** from Runtime #83's **12 profile/setup + 14 cohort methods**, and the mozlog **21 methods + 36 independent diagnostic cases**. The separate mozlog observations contain 14 expected acceptances and 22 expected rejections. Null/empty/whitespace subtests are correctly allowed, raw nonstrings rejected, and the 2-GiB gate is accurately described as a mocked-stat predicate rather than a physical fixture. The independent setup report substantiates the complete 26-config byte comparison: five canonical Debug, thirteen production and eight explicit legacy Debug+PGO combinations. “Before setup actions” refers to the setup script, not every preceding workflow operation.

The retained clean Floorp receipt records head `7747cf0e…`, the stated runner SHA256, 106 cases, all three zero exits, source PASS and no product/native authorization. `public-ci-confirmed-01.json` agrees with the four documented run/head/attempt-1 observations. The draft correctly distinguishes PR merge-checkout SHA from PR head, required job/step success from conditional skips, and metadata/log verification from inaccessible artifact bytes. The two materialization records are HTTP 403; full CI receipt ZIP bytes and exact Linux browser assertion count remain unverified. This review checked retained observations; it did not fetch current CI status.

README's 114/130/162/16/106 counts describe the historical kit-repair phase, and its new link identifies the later six-finding follow-up. The preserved `runtime-count-reconciliation-01.json` states that the actual pre-parent-init whole Runtime run at `cc83e6f…` was **190**, not the earlier commentary's 191, with the original raw log hash retained. The current draft makes no false 191 claim. Final Runtime #84 head/count, parent-initialization and strict private-protocol acceptance are still pending; absence of their final paragraph is unfinished author work, **not** a defect in this candidate. They must be filled from the final raw suite and independent review before packaging/final-head claims.

Ordinary existing public GitHub-hosted PR CI is correctly separated from the unexecuted private full Debug proof and new paid runners. Neither cancelled historical integration run is promoted to PASS. Existing source shadow remains nonblocking; broad workflow success is not a raw source verdict. No retries or qualification authority are introduced by these docs.

## Public selection and packager

[Static inspection](docs-public-static-inspection-01.json) independently reproduces only the source-selection/read checks, without running packaging. At that inspection it selected **286 regular UTF-8 files**, totaling **2480707 bytes**, with maximum member **318142 bytes**. This count is an observation of the pre-final source list, not the future archive count. Later review evidence or pending Native repairs will change it.

- The two original URL-bearing `.error.log` files are excluded; only separately named query-redacted replacements are selected. The only selected Azure blob URLs are in those two replacements and have no query. No selected URL has the packager's credential query keys, and no GitHub-token shape or private-key header matched.
- Only `fixtures/run_checks.py` is admitted from the fixture directory. The live private download descriptor, materializer program and other `fixtures/private-*` files are outside selection.
- Other evidence directories, including native/process scratch trees, test fixture trees and setup virtualenv/cache trees, are outside selection. The explicitly admitted `floorp-source106-clean-commit-01` subtree contains logs/XML/receipt only. No selected symlink, cache path or non-UTF-8 file was found.
- Lines 48–49 refuse existing/symlink destinations, and writes use exclusive creation. Data is captured and hashed before copying; relative evidence paths and deterministic regular ZIP metadata are used. The program has member/byte caps plus final CRC/path/type/encryption/expanded-byte checks. These are static program properties, not a completed packaging receipt.

The credential scan is a bounded pattern check, not proof that arbitrary future file additions contain no secrets. The selector admits every top-level regular evidence file: the final frozen source list and generated archive/index still require the author's final scan/verification. Current selected content presents no confirmed P1/P2 exposure. Absolute workspace paths are explicitly historical context and do not assert reusable materialization locations. Native/product qualification and publication authority are explicitly absent.

## Disposition and limits

Historical-input preservation and the current public-selection definition are **Go within this scoped static review**. Documentation is ready after correcting P2-DOC-01 and inserting the still-pending final Runtime #84 acceptance/head/count and relevant final-head CI record. A new archive is not yet verified by this review. The separate code review of the newly frozen Native cleanup repair remains to be performed; no private launch or qualification/publication Go follows from this document review.

The inspection used a credential-free Git-read environment, 256 MiB address space, CPU 20 seconds, 4 MiB per input/write cap and bounded Git reads. Its first diagnostic invocation mistook the historical `files` mapping for a record list; the diagnostic-only failure is retained in `docs-public-static-inspection-01-failure.log`, then the reader schema was corrected. Sources and historical evidence were not modified. No packaging/CLI/installer/browser/model/network operation was executed.
