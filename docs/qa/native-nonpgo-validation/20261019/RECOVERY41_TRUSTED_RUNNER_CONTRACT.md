# Trusted runner checkpoint contract

This checkpoint is QA state transfer between two serialized standard GitHub `macos-15-intel` jobs. It assumes trusted, pinned build inputs and a single purpose runner with no concurrent user build, deliberate detached output writer, or hostile same-UID process. The requirement is natural completion of this build and a verified, consistent archive. Atomic filesystem snapshots and immutability against arbitrary same-UID mutation are neither prerequisites nor guarantees.

## Explicit change from the previous gate

The producer previously required every new effective-UID or real-UID candidate since its initial snapshot to disappear. It then measured all those candidates' task counters while waiting. Process birth after the baseline does not establish build ownership or output writing. Conversely, an older process can write. Therefore the global delta-zero predicate was not a proof of build completion and could hold up a completed build on unrelated macOS work.

The new versioned contract is `trusted-single-purpose-runner-v1`. It replaces that global wait with all of these conditions:

1. Every preceding Runner stage exited naturally with code 0, no recorded stop reason or failure phase, an exact reaped Popen leader with code 0, a verified absent owned process group, and no cleanup failure. There is no first failure or monitor failure and the active group set is empty. Configure, effective configuration, exports, Rust target and archive inspection must each appear exactly once. A stop followed by exit 0 is still ineligible.
2. The existing sampled history of owned stage-group birth identities is available and not truncated. One fresh, self-bound full kernel process snapshot checks those known births, independently of their current credentials or process group. Any known live process or zombie rejects the checkpoint. A valid different birth is a different generation; a record for a known PID with missing or invalid birth/state remains a failure. Absence of that PID from a complete snapshot is consistent with exit. This registry is sampled history, not a claim of complete ancestry coverage.
3. The selected Rust graph node and fresh archive/dependency outputs satisfy the existing graph and format checks; host TU and XUL are still absent at this producer boundary. Their recorded hashes must match the captured object-tree manifest.
4. The saved roots and exclusions are fixed by the existing manifest contract. There is no build restart between completion and capture. Every payload byte passed to tar is hashed through its actual descriptor stream with metadata and identity checks. The complete selected payload and source/tools/SDK reference trees are rescanned after archive readback, including paths, kinds, modes, times, links and hashes. A late file, changed output or reference rejects readiness.
5. Transfer binds the single archive and manifest digests to the same run, attempt, source, fixture, harness and environment. The consumer verifies every archive member and complete payload, restores typed links/modes/times, compares complete restored trees and source/tools/SDK reference content, and checks effective build configuration before resuming.

The full kernel snapshot is used only to check known build births. UID membership alone establishes neither build ownership nor writer status, and ambient task counters are not requested. Unmeasured background usage is explicitly `NOT_MEASURED_OUTSIDE_DECLARED_OWNED_SCOPE`, not zero. No process-name exclusion is introduced.

## What remains unchanged

Resource monitoring for each active owned process group, limits on RSS/disk/logs, unavailable-sample failure, stall handling, exact ownership checks and bounded cleanup remain unchanged. The replacement eliminates the artificial post-build global-UID wait and its unrelated-counter scope; it does not discard an active owned-group monitor failure. Existing per-phase and platform deadlines, stored/logical size caps, single artifact, one-day retention, source/toolchain/build flags and authentication criteria remain unchanged. SDK acquisition still uses the normal unexpired API, HTTP error stop, acquisition deadline and fixed archive/tree/version validation. No product signing, permission changes or publication flow is added.

The fixed source mozconfig leaves sccache/ccache options commented out, and the build environment does not inherit cache-wrapper variables. The trusted build recipe is not configured to deliberately keep a detached compiler or writer alive. A future recipe that adds such a service must define and track its shutdown before it can use this contract.

## Interpretation of Recovery40

The held-FD and shared-mapping controls intentionally kept a writer alive. They demonstrate why a global birth delta is not a writer proof. They do not demonstrate a writer surviving a naturally completed, reaped build with absent owned groups and no known surviving births. Mapping and descriptor references disappear when their last owning process exits; the completed-writer roundtrip test covers capture after a child has closed the mapping and been exactly reaped.

The read-only image experiment established ordinary filesystem operation refusal for its fixture, but backing write-open capability was not excluded. This is useful diagnostic evidence; no read-only image or hostile same-UID immutability requirement is added to the QA route.

## Residual risk and qualification

A very short-lived child could escape group membership before a sampling interval and deliberately retain a writer, or an unrelated trusted service could actually modify a selected root. Group/known-birth checks do not prove that impossible. Full pre/post and archive/extraction comparisons detect ordinary persistent changes but are not a linearizable snapshot and cannot rule out an adversarial change-and-restore race. The contract accepts these limits under the stated trusted runner and recipe assumptions. An unexpected mutation still fails capture; interrupted or unknown completion is never eligible.

Short fixture tests establish implementation behavior, not a completed Floorp build. The subsequent long validation must separately establish producer natural completion, verified transfer/restore, full host TU/XUL/package, and native Shim authentication. Earlier native exit causes remain unknown. Recovery39's ended worker cleanup remains unknown; later successful fixtures do not reconstruct it.
