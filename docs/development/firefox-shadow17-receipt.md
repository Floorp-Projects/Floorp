# Shadow17 offline receipt contract

This proposal adds only a structural verifier and host tests. It does not
create a workflow, install or start a runner, fetch an artifact, authorize a
native execution, publish a check, or qualify any result.

## Fixed scope

State12 and History5 are separate serial cohorts, with exact case IDs and
reviewed selection digests pinned independently in the verifier. The source
baseline is Floorp `7b89a91d51534eeec9d591740f808dd49bb6133b` with strict
component Runtime `6ffed8ad4dcfadeb85bb158c82395cefcc3e2c06`. The ordinary
Floorp Runtime lock pins `741c4537…`, which is not a substitute. Preserve the
original manifests, registrations, harness, assertions, expectations, timeouts,
guards, fixtures and support files.

The original case IDs were independently compared with declarations and
terminal IDs in a projection from existing local logs: 12/12 and 5/5. This
does not establish full raw evidence, hosted provenance, production identity
or a passing check. The test file uses only synthetic logs and placeholder
material/transport IDs to exercise the offline verifier.

Run these host-only controls with the existing QA3 discovery layout:
`python3 -B -m unittest discover -s tools/qa3 -p 'test_*.py' -v -k Shadow17Tests`.
The existing QA3 source-shadow discovery also picks up the file on eligible
PRs if this patch is ever published. It does not alter the fixed source
execution plan or introduce any native mechanism.

## Narrow future interface

One reviewed producer is identified by repository, run, original attempt and
numeric job. A direct dependent observation supplies these same fields and
exactly two fixed slots, each with a distinct artifact ID/digest. An exterior
observer supplies per-slot process exit, producer identity, profile identity and
material observations. Each slot is compared with an independently supplied
policy and the digest of the supplied exact ZIP bytes. A supplied validator
checks each original stream independently and cannot grant qualification.
Unknown or missing roles, swapped/duplicate artifacts, wrong attempts,
inconsistent material, and either cohort's failed assertions or process exit
deny structural acceptance. No merging of disposable profiles or retries.

The common support digest is the reviewed complete **union** of installed
harness/support and material needed for both roles on the shared immutable
binary/overlay, not a claim the two manifests declare identical fixtures.
The two manifest digests and two fresh owned-profile identities are distinct.

## Before any hosted adoption

This verifier accepts in-memory inputs; it does not authenticate their origin.
An independent collector must establish immutable reviewed workflow identity,
actual exact-attempt authenticated API reads and genuine direct-dependency
outputs, independently authorized policy, and frozen bytes and capture from a
supervisor outside candidate authority. Exact identities and complete original
leak/shutdown records and process exits remain mandatory. A complete original
logger/leak adapter and authentic positive and negative hosted evidence remain
outstanding. No change to release workflows, current required checks, CSP or
branch policy follows from these structural controls. Even a consistent
result always has `authenticated=false`, `native_pass=false` and
`qualified=false`.
