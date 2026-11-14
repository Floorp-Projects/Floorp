# QA archive byte integrity — verified, nonauthorizing boundary

## Implemented scope
`verifyFinalPackageQaArchive` accepts provided ordinary Uint8Array bytes plus caller-provided artifact/expected identity and SHA-256 metadata. It performs no filesystem, network, native QA, credential discovery or publisher operation. Metadata API authenticity remains the caller's responsibility.

- Metadata primitive fields are read once. A private fixed-buffer byte snapshot is copied before the first asynchronous SHA-256 operation. Shared/resizable buffers, subclasses/Proxies and shadowed typed-array size properties are unsupported. Node Buffers must be normalized by a trusted bounded caller to an ordinary Uint8Array; the helper does not silently accept subclass behavior.
- SHA-256 of actual supplied bytes must match the captured expected digest before ZIP parsing.
- A narrow ZIP32 profile validates central/local headers, flags/method/version/names/times/sizes/CRC and contiguous physical extents. Data descriptors are interpreted from exact 12/16-byte extent, not a greedy signature guess. Overlaps, gaps, duplicate/unsafe/case-colliding/prefix-colliding paths, special files, comments, extras, encryption, multidisk and ZIP64 are refused. This is intentionally not a general-purpose ZIP reader.
- Every entry, including discarded logs/images, undergoes bounded expansion, exact decoded-length and compressed-consumption checks, plus independent CRC32 verification. Only the two exact root JSON objects are retained in the result.
- Required JSON uses the actual workflow serialization: UTF-8, no BOM, JSON.stringify(value,null,2), optional one LF. Structural depth is prechecked, then native JSON parsing and canonical roundtrip reject duplicate/escaped-conflicting keys and noncanonical alternatives. Valid but differently serialized JSON can be rejected by this narrow contract.

## Fixed budgets (not a hard process-heap claim)
| Quantity | Limit |
| --- | --- |
| Compressed ZIP | 8 MiB |
| Aggregate declared/actual expanded bytes | 16 MiB |
| One extra entry | 8 MiB |
| Each required JSON | 1 MiB |
| Entries | 64 |
| ASCII path bytes | 240 |
| JSON structural depth | 64 |

The implementation checks metadata budgets before expansion, then verifies actual bounded output. Concatenation may transiently double accepted inflater output, and runtime/native window state, compressed snapshots and parsed JSON objects add overhead. These numbers are byte budgets, not proof the total heap stays below 16 MiB. Extras are processed sequentially and not retained.

## Backend decision and evidence
Installed fflate 0.8.3 unzipSync trusts originalSize for allocation; its fixed-buffer inflater can silently clip, and streaming callbacks occur after allocation. Those APIs were not adopted as a hostile-input memory boundary. No new DEFLATE parser was written.

The helper instead uses maintained node:zlib inflateRawSync with explicit chunkSize 1024, maxOutputLength bounded by the declared/hard cap plus one, info:true shape validation and exact engine.bytesWritten equality.

Exact runtime source examined:
- https://raw.githubusercontent.com/denoland/deno/v2.9.4/ext/node/polyfills/zlib.js — processChunkSync checks cumulative output before allocating the next chunk/concatenation.
- https://raw.githubusercontent.com/denoland/deno/v2.9.4/ext/node/ops/zlib/mod.rs — native start_write bounds the caller-provided output slice and sets zlib next_out/avail_out.
- https://raw.githubusercontent.com/denoland/deno/v2.9.4/ext/node/ops/zlib/stream.rs — maintained native inflate call, not a whole-expanded-output Vec.

Independent Deno 2.9.4 probes confirmed output-limit rejection (not truncation), strict consumed input for trailing/concatenated stream rejection, invalid LEN/NLEN/truncation rejection and bounded observed JavaScript allocations before a synthetic bomb rejection. This is source/runtime evidence for the tested backend, not a universal guarantee for every future runtime or every native allocation.

## Verification

```sh
deno test --cached-only --frozen tools/src/fdh_final_package_qa_archive.test.ts
deno lint tools/src/fdh_final_package_qa_archive.ts tools/src/fdh_final_package_qa_archive_types.ts tools/src/fdh_final_package_qa_archive.test.ts tools/integration/fdh-qa-archive-samples.ts
# Explicit historical compatibility check only; operator-provided local ZIPs required:
deno run --cached-only --frozen --allow-read tools/integration/fdh-qa-archive-samples.ts
```

- Final repository regression file: 108/108 pass offline without grants.
- Independent reviewer ran 10 fresh spoofing/byte-kind probes on the final candidate; these are not extra committed regression cases. Earlier 21 probes belong to the superseded 102-test review.
- Metadata getter substitution was reproduced and repaired with eight capture/accessor regressions. A later fully forwarding Proxy bypassed visible type/size checks, so that 102-case approval was also withdrawn. Native ArrayBuffer.isView now rejects Proxies before traps/copy; a cached native element-kind getter and ordinary backing-buffer prototype enforce exact byte semantics. Six added byte-boundary tests cover forwarding/size-iterator Proxies, backing prototype/subclass and forged Uint16/Clamped prototypes. The latter element-kind edge is not characterized as an allocation exploit. Prior 94/102/106-case verdicts are superseded by fresh 108-case acceptance.
- Full canonical FDH host suite after final native-byte guard repair: 939 passed / 0 failed / 3 ignored.
- Helper SHA-256: C0C2C4E93766F2BF446FDC0AA7F799E42D5FBE4A78D35972381493DB4D182970. Types: 3657F36D5C8E884D702BCE751997B434596241240439FD45B03314339729BC99. Tests: 7CD0C5D1EB219C0BC4C9CC4C04F918E4EAE91604BB44EA59A1291FCFCBFF21C8.

## Historical real-archive compatibility
Existing QA artifacts from https://github.com/Floorp-Projects/Floorp/actions/runs/43363023598 were obtained through read-only gh API calls, exact IDs/names/source checked, and actual ZIP SHA-256 matched returned metadata before header inspection. No payload was executed, no native QA rerun, and no workflow triggered. Public metadata inspection is not a new live approval test. The tracked integration script never downloads anything and fails if local samples are absent.

| Target | Artifact id | Compressed bytes | Entries | Expanded bytes |
| --- | --- | --- | --- | --- |
| Windows x86_64 | 14009161864 | 249929 | 8 | 1338704 |
| macOS x86_64 | 14008869255 | 318643 | 19 | 1441026 |
| macOS aarch64 | 14008634582 | 318829 | 19 | 1438915 |
| Linux x86_64 | 14008374789 | 260685 | 8 | 1349099 |

All four historical samples passed the final helper in both lead and independent reviewer execution. They use method8, flag8, zero local sizes, signed32 descriptors, neededVersion20, no comments/extras, two canonical root JSON files and safe bounded extras. Empty DEFLATE extras on macOS are supported. These samples do not establish compatibility with future ZIP formats.

## Explicit non-guarantees
Success reports archiveIntegrityMatched:true but publicationAuthorized, apiAuthenticityVerified, nativeQaVerified and liveApprovalVerified remain false. There is no automatic composition with the previous consistency helper or publisher. Complete trusted API collection, current-attempt/source eligibility revalidation, QA3 policy, protected approval and live integration testing remain separate work. Production workflows, eligibility logic, settings and Jira completion state are unchanged.
