# Checkpoint boundary assessment after Recovery40

The short Intel Mac diagnostic completed successfully in [run 39181139776](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/39181139776), using fixture `361b16aec48366e17e752dc833c38341a901c596` and caller `51d05dfe8a838a188fe64ae8bd42295f83da03dc`. All 84 target tests passed without skips. The run performed no SDK acquisition, Floorp build, signing, or native authentication qualification, and saved no Actions artifacts. Long checkpoint execution remains **NO_GO / ON_HOLD**. Production eligibility and resource gates are unchanged.

## Capture and writer eligibility

The previous archive implementation could omit a file added after its initial manifest enumeration while still passing archive inspection. The revised capture hashes the exact descriptor stream sent to the archive, checks file identity and metadata around that read, and fully reenumerates payload and reference roots before reporting readiness. The retained counterexample now fails with `checkpoint-state-changed-after-pack`; descriptor mutation and late reference/state additions also reject readiness. These checks improve detection but do not establish an atomic, globally consistent filesystem snapshot.

A global rule that every newly observed same-UID process is a writer is neither necessary nor sufficient to establish the absence of writers. Recovery40 positively demonstrated that a controlled child already present in its dedicated baseline could change the target while retaining the same full identity and without becoming a new candidate. This held for both an open writable descriptor and a shared writable mapping whose descriptor had been closed. These controls did **not** assert or retain that the global all-candidate delta was zero. No process-name exclusion or attribution of earlier candidates follows from them.

## Read-only image experiment

Each held writer caused ordinary, non-force detach to fail with utility exit 16 while the original writable mount remained attached. No kernel errno was established for that utility failure. After the exact children released their references naturally and were reaped, normal detach and read-only reattachment succeeded.

For the same ownership-bound fixture, stable positive kernel `ST_RDONLY`, `O_RDWR`/create/rename refusals with `EROFS`, and unchanged payload and manifest hashes established protection against those ordinary filesystem operations. A shared writable mapping from a read-only descriptor was refused with `EACCES`; that refusal alone is not independent proof of filesystem read-only enforcement. Private mapping changes did not change the file. The owned image was finally verified not attached, using cleanup that does not depend on read-only property availability. Process cleanup and image cleanup are recorded separately.

Opening the backing file with `O_RDWR` succeeded. No raw write or writable reattachment was attempted. Consequently the experiment does not establish backing immutability against the same UID, nor a protected production checkpoint. Freezing all production payloads and source/tools/SDK references, preserving the required layout and typed links, establishing a natural build boundary, and specifying writer/resource ownership remain unresolved. Read-only target protection does not justify treating unknown CPU/RSS or process accounting as zero.

The current worker exposed `Writable` as a typed Boolean but omitted six other queried diskutil properties. Kernel flags were used explicitly; missing properties were not guessed or converted to a false value. Recovery39's earlier schema failure and unknown image cleanup remain unknown for that ended worker; Recovery40 does not retroactively establish its exact property values or cleanup.

## Native process API observations

Controlled natural Python exit transitions produced 20 samples where full BSD flavor 3, argument 1 observations matched the same live five-field birth identity around task-all-info flavor 2, argument 0 returning zero with `ESRCH`. No qualifying sample established the full before/after/spawn-anchor exec binding; in the retained first example the trailing unique query returned `ESRCH`. Task-info flavor 4 and unique flavor 17 also returned `ESRCH` during transitions; neither is promoted to fallback authority. Unreaped zombies were separately recognized and exact children were reaped without signals.

This establishes an actual exit-window API pattern. It does not identify the natural cause in Recovery35, reproduce its complete outer query sequence, or demonstrate that the later delayed retry repairs that failure. Unavailable observations remain unknown. The outer monitored diagnostic stages themselves completed without monitor failures.

## Daily Intel native connection evidence

The separate [daily Intel job](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/39162639256/job/117820798341) passed preliminary startup and reached the context-test Marionette session, then timed out waiting for `connected`. The host still answered a state request after the timeout; empty crash/failure arrays do not establish whether the Shim exited, rejected hello, or hit a timer. The actual native exit cause remains unknown.

The minimal next recording change is a bounded, allowlisted pre-cleanup observer event history plus separate host, launcher, and child-monitor exit snapshots, persisted independently even if cleanup fails. Requested cleanup signals and post-cleanup errors must remain distinguishable from the first observed cause. This is a recording proposal, not an implemented native authentication fix. Its detailed source and artifact bindings are in `RECOVERY39_DAILY_HOST_STATE_REVIEW.json`.

## Remaining execution boundary

`RECOVERY40_TERMINAL_RESULT.json` retains the complete safe diagnostic object, source bindings, provider-log digest, and explicit limits. The independent terminal review is recorded separately. The successful fixture test does not authorize a production gate replacement. Long full host/XUL and native authentication validation remain pending a reviewed checkpoint and resource contract. Any future SDK acquisition must recheck the ordinary API expiry and retain the existing HTTP failure, acquisition deadline, and fixed archive/tree/version checks; no expired retrieval or retention change is implied.
