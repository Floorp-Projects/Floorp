# Owned-image diagnostic correction

Recovery39 remains failed. Its77 actual Mac tests passed and its32 controlled
children completed. In nine exit-window samples, full BSD PID/birth/credentials
remained bound with live state2 while task-all-info returned0/232, errno3/ESRCH.
The first example changed from INEXIT=false to true, and task-info also returned
ESRCH. The trailing unique/exec query was unavailable, so exec-bound examples
numbered zero. This narrows a controlled API behavior, without establishing the
natural Recovery35 cause or effectiveness of its outer delayed retry.

Recovery39's image creation and RW attachment commands succeeded. The probe then
required two unobserved diskutil plist properties to be booleans and failed with
disk-readonly-unavailable. It did not retain which property was absent or of a
different type. Writer, held-descriptor, mapping, read-only and backing controls
were not reached. The same property check blocked image cleanup: detach is
unknown, despite all six monitored process groups being gone. No later run can
reconstruct the ended runner's attachment state.

Recovery40 corrects only the private diagnostic:

- Separate positive image ownership binding from read-only interpretation.
  Bind the unique owned backing inode, exact image path, registered device set,
  VolumeUUID, canonical mount and mounted-root identity. Ordinary cleanup uses
  that binding; it does not require diskutil RO properties. Unknown identity
  still forbids detach. No force, sudo or unrelated device operation is added.
- Retain only a fixed set of diskutil property presence/type/boolean values,
  plus the stopping binding observation before cleanup. Unknown properties stay
  unknown; no unobserved schema alias is guessed.
- Define this fixture's mounted-filesystem protection through the kernel
  statvfs ST_RDONLY flag on the fully bound mount. Require its positive, stable
  value before/after ordinary write-denial and hash checks. Unknown/invalid
  flags, non-RO state or identity changes fail. diskutil media attributes remain
  separate diagnostic observations, without inferred semantics.
- Keep exact child reap, outer monitored process-group completion, command and
  output limits, non-force image cleanup and separate cleanup failures.

Successful controls mean only that this private fixture's ordinary filesystem
writes were refused under the observed kernel RO state. They do not establish
read-only media, same-UID raw-backing immutability, absence of remount capability,
an atomic filesystem snapshot or production checkpoint eligibility. A raw
backing open is still only open/close, without any raw write. Every candidate
and every required resource measurement remains under the existing production
contract. The production census, resource limits and checkpoint gate do not
change; the capture hardening reviewed in Recovery39 stays fixed.

The one standard macos-15-intel diagnostic job has a20-minute platform budget,
84 targeted tests with zero expected skips, a controlled task-query observation
and one image experiment. It performs no SDK acquisition, Floorp build, artifact
upload, product signing, release or authentication/permission setting change.
The SDK's observed expiry remains2026-10-19T10:00:38Z; any future acquisition must
be checked through its ordinary API without expiry bypass. Long execution is
NO_GO_ON_HOLD and does not start automatically from this short result.

The separate daily native investigation and its minimal pre-cleanup recording
proposal remain in RECOVERY39_DAILY_HOST_STATE_REVIEW.json. No new host or Shim
authentication result is claimed here.
