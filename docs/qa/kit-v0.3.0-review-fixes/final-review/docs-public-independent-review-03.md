# QA309 final documentation/public archive acceptance — scope Go

**Go for the reviewed documentation and public evidence archive. No remaining confirmed P0/P1/P2 in this scope.** P2-DOC-01 remains closed. This is not Native/product qualification, publication approval or a claim that the still-unpushed final documentation head has passed CI. Source/code acceptance is attributed to the separate fixed-hash independent reviews.

This report and its new inspection evidence are deliberately outside both the frozen public copy and `fix-qa309/evidence`. I made no source/document/archive/phase-evidence changes, CI/network operations, packaging, extraction or bundled code execution. The only execution was the bounded independently written read-only archive inspector.

## Accepted immutable inputs

| Final input | Bytes | SHA256 |
| --- | --- | --- |
| FOLLOWUP_QA309_Evidence.zip | 782560 | `2c91b368a2f03ef4ec3267575ded489c4c5827d9db62810567b365132f397d17` |
| followup-evidence-index.json | 65825 | `d862a8aea7f2851d340d596a318bc0a1dc89e63b545a106bc7fb0c89ec313416` |
| FOLLOWUP_QA309.md | 15732 | `ee6f6d4ff30f89e0866ff72349fdec9e2b9afefc38fd4803bc264abe515ec5d7` |
| README.md | 10649 | `78651eb5978e0b152f6ea16ef41bd9bb273a234603cbbb3c4ab0ed708b6ea9fa` |
| Local .gitattributes | 143 | `b83cb5e128902c60041c5ca2383b50e98a44a0b1b8ef9dfed1500295cecef98f` |

[Complete inspection](docs-public-archive-inspection-03.json) verifies **347 unique entries**, **3790824 expanded bytes**, exactly **345 indexed data files** plus the report/index. Every indexed entry agrees in size, SHA256 and full bytes with both its public filesystem copy and original phase evidence/selected fixture source. The copied directory contains exactly that indexed set, with no extra file. The report and index inside the ZIP match the external files. CRC passes; every entry is a relative canonical regular file with mode 0644, no traversal/backslash/NUL/duplicate/encryption or symlink. The external archive receipt matches these independently measured values.

All entries decoded as UTF-8 and passed the declared credential-query, GitHub-token and private-key-header scans. No selected cache path, live private descriptor or original temporary-URL error file is present. The only Azure blob URLs are in the two separately query-redacted error logs, with no query. This is verification of the fixed archive against explicit patterns and source selection, not a claim that a general-purpose secret detector has proven arbitrary content safe. No live URL was printed in new review evidence.

Original input ZIP, repair ZIP, QA308 report and its 427-member evidence ZIP still have their frozen lengths/hashes:

- Original 150471-byte input: `a4af60380399050f82465864734391227ab34024d26bdf16c31559e2824a8d50`.
- Repair 158581-byte input: `60ad9a63c9fa7e328cc2da41187c6b725a64ee6144aaac8f66c2ebc3260710f2`.
- QA308 24502-byte report: `26b8b073b2cdf3793b57f858634ecb150b6c4150c033b4f514ff2170a0bcc84f`.
- QA308 2238033-byte ZIP: `60b5386d5363edafc4fac738e32c7dd59336db2d23fffb2ba8940a81f65c91d2`.

The unchanged kit ZIP hashes preserve the prior independently verified DESIGN/14-document byte equality. Initial RED and candidate-01/candidate-02 review data remain separate immutable archive entries; this new final Go does not rewrite their historical conclusions or failed fixtures.

## Documentation facts and boundaries

The final follow-up now distinguishes the complete local **203 methods / 56.217s / zero skips** at `c9027879e053c9031c7f3e27495f6d415d36c10f`, the previous **190**-method broad run with its still-blocking parent-init fault, the peer review's **47 actual helper methods plus five independent process-fault cases**, and the second peer's **71 synthetic/control-policy cases**. The final native source/test archive snapshots match the stated SHA256 `8bbea4ab…` / `0dae5c08…`. The corresponding peer reports substantiate their separate executions and limitations; neither broad success nor protocol-only fixtures are substituted for the actual cleanup observations. I did not independently rerun these suites in this archive task.

The final-head CI addendum and [binding check](docs-public-count-binding-03.json) agree on Runtime #84 head c902, PR/base #83 `91e94b7361…`, `.github/workflows/qa3-shadow.yml`, run **37954821322 / attempt 1**, job **113902544655**, required steps successful, merge checkout **7e4b7ce818aadc5cfd41f58a79b82ed3487d4b6f**, and raw **134 QA3-subset tests / 49.212s / OK**. The raw log matches `9bff23cd43eb073f2b67a711c71d724ac7ba5ce093c5c7655afca3e07ae4de45`. The draft correctly does not call that 203 tests or derive PASS from the nonblocking workflow status alone. This is a check of retained official-connector observations, not a new fetch of live CI state.

Floorp's 22 contract methods and 106 unchanged source cases, #83's 12+14 methods, and mozlog's 21 methods/36 diagnostics retain their distinct contexts. The 14 local relative links in the final follow-up all resolve. Artifact HTTP403 limitations are still explicit: inaccessible CI ZIP/receipt bytes and exact Linux browser assertion count are not claimed verified. Full modified-file lint limitations and missing Shellcheck remain disclosed.

`FOLLOWUP_QA309.md` lines 197–207 and README lines 140–146 retain the corrected **future** QA3 ordering, explicitly state that it is not wired into the existing publisher, and preserve existing manually-public release checks. Source shadow is nonblocking; product/native qualification remains `UNVERIFIED` and publication authority false. Existing ordinary public hosted PR CI is distinguished from unexecuted private full Debug/paid proof. The final documentation-head CI is explicitly future work after commit/push, with no older/cancelled result substituting for it. No broad release, all-target or external worker-isolation readiness is claimed.

## Raw-byte preservation and disposition

The staged `.gitattributes` difference is exactly one addition, `followup-evidence/** -whitespace`, under this local documentation directory. The original `*.log`/`*.patch` rules remain unchanged. The README explains that these are frozen data snapshots; active implementations and explanatory documents retain normal diagnostics. Full byte comparisons above show that data was preserved rather than stripped or reformatted to silence whitespace errors. This exemption does not alter active QA code, workflows or gates.

The fixed document and archive are review-ready for the already-authorized documentation Draft update and ordinary PR CI observation. Final #2893 head/run/attempt and required job outcomes still need the parent's post-push observation. This scoped Go does not remove known qualification/trust/private-worker/publisher prerequisites or authorize private/paid dispatch, native/full builds, signing, merge, release or publication.

The inspector used 256 MiB AS, CPU 20 seconds, 4 MiB per regular input/write, no core dumps, bounded archive sizes and no extraction. Its invocation exited 0; [reader source](docs-public-archive-inspection-03.py), per-member hashes/URL metadata and [count/CI binding result](docs-public-count-binding-03.json) are saved outside the frozen inputs. No failure or source mutation occurred in this final inspection.
