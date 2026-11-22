# Offline QA evidence composition — verified, not live readiness

## Public contract
`verifyFinalPackageQaEvidence` connects the accepted ZIP integrity and consistency helpers in memory. Its request contains only canonical pretty-two-space `expectedJson` and `observationJson` strings (optional one LF) plus exactly four data-property descriptors `{artifactId, bytes}`. Policy origin and API authenticity are NOT established by this input representation.

- Metadata: each <=1MiB UTF-8, combined<=2MiB, depth<=64 checked before native JSON parsing. Owned parse results and canonical roundtrip reject duplicate/ambiguous JSON keys/spellings.
- Observation has exactly packageRun, qaRun, qaJobs, packageArtifacts and qaArtifacts. Caller-provided decoded evidence, archiveVerified flags and unknown envelope keys are rejected.
- Four jobs, four QA artifacts and four unique archive mappings are required. Complete package-artifact inventory is bounded1..64, not exactly eight selected identities. Read-only inspection confirmed the real source run has32 entries, including unselected ARM/native/build/provenance/unsigned/MAR artifacts; duplicates and expired items must not be filtered away by a future collector.
- Request, dense array and archive descriptors are captured as data properties without invoking accessors or custom iterators. Native view/element/backing checks precede length aggregation or copy; forwarding Proxies cannot be normalized first. All four private byte copies and immutable metadata exist before the first await.
- Raw ZIP limits:8MiB each and16MiB aggregate. Four sequential accepted helpers can process up to64MiB decoded aggregate; parsed objects, input/private copies, concat and native state add overhead. These are not process-heap guarantees.
- Only successful actual `verifyFinalPackageQaArchive` input/sourceAfterQa objects construct the evidence array passed to the unchanged `validateFinalPackageQaConsistency`. No injectable verifier callback or self-declared decoded proof can bypass this path.
- Results return safe artifact summaries, not decoded payloads. Failure returns fixed scope/stage fields only, without arbitrary exceptions or partial successes.

## Proof layers and non-guarantees
Success reports `offlineEvidenceMatched:true` with an explicit `offlineArchiveIntegrity` layer and the **unchanged nested** `attestationConsistency` result. The older consistency helper's `archiveBytesVerified:false` is retained because that method itself does not check bytes; the independent outer byte-integrity layer is not presented as API authentication.

Transport/API authenticity, native QA, live protected approval and publication authorization always remain false. This function performs no filesystem, process, network, collector, browser or publisher operation. It assumes an uncompromised JavaScript runtime/intrinsics and does not sandbox arbitrary same-process code. Host/operator policy must never be inferred from the evidence being checked.

## Verification

```sh
deno test --cached-only --frozen tools/src/fdh_final_package_qa_archive.test.ts tools/src/fdh_final_package_qa_gate.test.ts tools/src/fdh_final_package_qa_evidence.test.ts
deno lint tools/src/fdh_final_package_qa_evidence.ts tools/src/fdh_final_package_qa_evidence_types.ts tools/src/fdh_final_package_qa_evidence.test.ts
deno fmt --check tools/src/fdh_final_package_qa_evidence.ts tools/src/fdh_final_package_qa_evidence_types.ts tools/src/fdh_final_package_qa_evidence.test.ts
```

- Composition repository regression cases:67 pass. Combined layers:260 pass (archive108+consistency85+composition67), offline without grants.
- Independent reviewer additionally ran33 in-memory adversarial assertions; these are not extra committed regression cases. Includes rehashed payload poisoning, wrong run/head/probe/target/signed/post/path, API attempt/repo/job contradictions, duplicate artifacts/JSON keys, accessor/size/backing/iterator behavior and late policy/byte substitution.
- Canonical FDH host run:1006 passed /0 failed /3 ignored. Scoped lint/format3 pass.
- Reviewed SHA-256: helper AAF024D22CA01E0A72BE0D7382B8D5584235BBCE26DFA155A2ACF17B48B2EF80; types40B0CB70AC69360042CCD63144C25CA89ADD90B126774D30F45EDEC6EAF4780E; testsE42D0F2F51686D5DEEF70B54B84CBC04E757AB777EC73C651FA54E2B0F19BDED.
- Prior archive helper C0C2…2970 and consistency C853…CAD6 are unchanged. No production workflow, eligibility engine, lockfile or setting changed.

## Next boundary
A canonical GET-only, bounded read-only collector must acquire complete current-attempt API metadata and the exact four archive IDs, obtain versions from package.json/runtime lock at the pinned source head (not payload), recheck state after collection and invoke the authoritative source eligibility helper. Existing source run43359316284 is now completed/public with no pending deployment; it is a valid live negative, NEVER a fabricated pending positive.

Only after trusted collection, fresh eligibility and a separately reviewed integration may a workflow consider these data as prerequisites. This module grants no release permission and closes no Jira/v1.0 AC. Windows native token support, native browser/macOS execution, OAuth and protected settings remain external/unfinished boundaries.
