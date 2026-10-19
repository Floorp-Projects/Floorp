# Checkpoint boundary diagnosis

Recovery39 is a short diagnostic, without SDK acquisition, a Floorp build or an
artifact upload. It does not replace the production checkpoint gate, change
resource accounting or authorize a long execution.

## What is now established

The current gate rejects every new effective/real-UID candidate relative to the
original baseline. A candidate's name or PID1 parent is not evidence of file
write capability or build ownership. Recovery38 observed new candidates without
a Floorp build, and also observed a return to strict zero. Thus strict zero is
possible in the short setting, but new candidates are not by themselves evidence
of a residual build writer.

The gate is not a complete proof of filesystem quiescence: a process already in
the baseline can write; a new writer can appear after the census; the census is
not an atomic snapshot of all processes and filesystem state. The independent
local counterexample adds a new object after manifest enumeration: the
pre-hardening implementation captured in the design review succeeds at
pack/inspect while omitting that object. This demonstrates a capture
limitation, without attributing any actual Recovery35 candidate or mutation.

Natural successful build stages, exact leader reap, group cleanup, fixed inputs,
manifest hashes, archive inspection and readback remain necessary parts of the
current contract. Individually stable files do not establish a coherent global
capture point. A process-name exclusion, current parent, no observed open file,
permission mode, advisory lock or directory clone cannot fill that gap.

The implementation now hashes the exact descriptor stream supplied to tar,
validates descriptor identity and metadata before/after reading, and re-enumerates
all reference/state roots after archive inspection before publishing ready. New
object, reference and state-root counterexamples, plus a modification during the
stream read, must reject saving. This strengthens detection and transfer
integrity. The final walk is still sequential; it is explicitly not an atomic
filesystem snapshot and does not justify weakening the census gate.

## The short experiment

Use a new private small APFS sparse image and only this experiment's exact child
processes. Record whether a baseline child can modify a target, whether ordinary
non-force detach rejects a held writable descriptor and a shared mapping whose
descriptor was closed, and whether a read-only reattachment rejects ordinary
filesystem writes from a waiting child. Preserve the observed API return and
mount/device/backing bindings. Do not assume all detach failures are EBUSY or
that all denied operations return the same errno.

The fixture checks a positive target protection mechanism. It does not claim a
filesystem snapshot or prove immunity to every same-UID process. A writable
backing-image open is tested only by opening and closing it, without writing.
Raw backing writes and a second read-write attachment are not performed.
Unknowns, unexpected detach behavior and cleanup failures remain explicit.

If ordinary read-only protection works, production adoption still requires a
separate design: all payload and source/tools/SDK references under the same
freeze, exact absolute-path and mount ownership/case bindings, typed links,
read-only verification through the end of packing, and repeated graph/archive/
depinfo/config validation from that frozen view. Same-UID backing mutation and
remount authority remain an unresolved trust boundary. The short experiment
cannot grant production eligibility.

Writer safety and resource monitoring are separate contracts. Protecting the
target does not make unknown candidate RSS/CPU zero or establish a new resource
ownership scope. Existing resource limits and unknown-query failure remain.

## Task-query diagnosis

Recovery35's first failure was checkpoint-census/monitor-unavailable, after the
Rust target and archive command naturally succeeded. Immutable code binds its
four unavailable task queries to libproc `proc_pidinfo`, flavor2
`PROC_PIDTASKALLINFO`, arg0, expected232, returned0, errno3/ESRCH. The BSD fallback
uses flavor3 `PROC_PIDTBSDINFO`, arg1, expected136; its complete identity matched
PID, birth seconds/useconds and effective/real UID. A live BSD classification is
not proof that task counters are available. The receipt did not preserve the
BSD state/INEXIT bit at that time; it cannot establish the kernel exit path.

The new diagnostic traces the existing production reader on controlled live,
exiting and unreaped children. It records API/flavor/argument/size/errno, query
order/timing, full-BSD identity comparison and an optional exec-generation
comparison. A separate task-info query is diagnostic only, never a fallback that
grants measurement or exclusion. No short run is guaranteed to reproduce the
natural Recovery35 window or demonstrate the delayed retry's effectiveness.

Apple XNU source explains a possible distinction between ordinary task lookup
and BSD zombie lookup, rather than the cause of the old event. Its source tag is
not an exact binding to the deployed runner kernel:

- [proc_info.c lookup and flavor handling](https://github.com/apple-oss-distributions/xnu/blob/xnu-11417.101.15/bsd/kern/proc_info.c)
- [fs_snapshot_create privilege requirements](https://github.com/apple-oss-distributions/xnu/blob/xnu-11417.101.15/bsd/man/man2/fs_snapshot_create.2)
- [Apple's APFS snapshot definition](https://support.apple.com/guide/disk-utility/view-apfs-snapshots-dskuf82354dc/mac)

Privileged filesystem snapshot APIs are not attempted. No sudo, force detach,
unrelated signal, authentication setting, retention or permission setting change
is part of this diagnostic.

## Separate native authentication investigation

Daily39162639256 reached verified context-test Marionette startup, invoked Shim
launch and then timed out waiting for connected. The host still answered the
exception-state query and cleanup script. That does not establish continuous
host liveness or any Shim exit status. The old cause-host-exited was a different
pre-adoption launcher-status classification with firstCause=null.

Existing daily evidence omits the observer event list, host kernel exit state,
launcher exit status and pre-cleanup child monitor snapshot. Child-side5-second
and host-side15-second timeouts precede the outer20-second wait; the recorded
timeout alone cannot identify which path ran. The smallest next recording change
is bounded allowlisted pre-cleanup events plus separate host/launcher/child
snapshots, preserved even if cleanup fails. Silent Hello rejection or launch API
failure may require a further fixed reason/API/OSStatus diagnostic. These are
recording proposals; no product or authentication change is made here.

The SDK reference remains artifact12133086104, observed expiry
2026-10-19T10:00:38Z. This diagnostic needs no SDK. Before any future acquisition,
recheck expiry through the ordinary API; expired acquisition is never bypassed.
Verified local SDK reuse is independent of build duration. No long run starts
automatically from this experiment.
