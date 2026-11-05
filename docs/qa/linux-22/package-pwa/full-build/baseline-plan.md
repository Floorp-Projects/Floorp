# Exact baseline production-package comparison

Requested baseline: Floorp `984509d2053be7bae9027133bda7259cd706b208`.
The live `main` branch had advanced to `29a3eb23145da90706d8a94441373278521cc737`
at the read-only preflight, so dispatching mutable main would not build the
requested comparison subject. A dedicated branch at the exact baseline is used.

## Audit and immutable subject

The exact baseline's Git blobs match the workflow/lock audited for both Drafts:

| Path | Exact blob |
| --- | --- |
| `.github/workflows/package.yml` | `0188bac2cd5fc0bef7ee40f2188dc9965641fe1e` |
| `.github/workflows/package_and_publish_release.yml` | `20a90fb25c0ef1b16b0a09b63a6617ff98f89d74` |
| `floorp-runtime.lock.json` | `8f3463c04e70cd900c55f4f1fd301403eccdf88a` |

The standalone workflow supports `workflow_dispatch` at that unchanged source.
Its required `floorp_head_sha` must equal the dispatch ref's actual `github.sha`.
The dedicated branch was absent at preflight (`baseline-remote-refs.txt`) and
was created without force or source edits at the exact requested commit.
No existing branch or shared checkout was overwritten.

## Authorized execution

The initial task was preparation only. Root then explicitly authorized creating
the isolated remote baseline branch and dispatching this same audited unsigned
Linux-only package route. The completed mutation is recorded here to prevent a
second dispatch of the already running subject.

- Ref: `codex/qa-linux-fullpackage-baseline-984509d2`.
- Created ref response: `baseline-ref-created.json`.
- Dispatch payload: `package-baseline-dispatch-payload.json`.
- Workflow: `.github/workflows/package.yml`.
- Run: [42068377281](https://github.com/Floorp-Projects/Floorp/actions/runs/42068377281).
- Observed head: `984509d2053be7bae9027133bda7259cd706b208`.
- Latest recorded state: in progress; this is not a package success or GUI result.
- Run metadata snapshot: `baseline-runs.json`.

Inputs are `platform=Linux-x64`, `beta=false`,
`provenance_mode=stable-v2-validation`, `skip_signing=true`,
`signed_validation=false`, legacy artifact-run field empty. There is no
publisher call, release/tag creation, merge, Runtime lock mutation or Done
transition in this task. This pipeline builds the whole Floorp application and
packages it with the authenticated prebuilt native engine; it does not freshly
compile all Gecko C++/Rust.

The baseline uses the same unchanged locked Runtime and successful canonical
producer as the two PR builds:

- Runtime commit `8965ffc346d426ccdf919bd18585e4d4a0c21654`, BuildID `20261103153232`.
- Canonical producer `41664957250`, daily-build.yml, attempt 1, completed/success.
- Manifest Actions artifact `13274392012`, digest
  `sha256:2b6a1b20190dc7016957ca25f72f8e83f267de51458b515153fd209bb90d9138`.
- Linux target Actions artifact `13272530069`, digest
  `sha256:ed5a4f97ee7bf42c817f3146d5a843e095710dcc3be25fae0af0bf47274b739a`.
- Both observed nonexpired, expiry `2027-02-01T15:32:34Z`.

Live Runtime and PR input readiness was supplied by the sibling package agent
and recorded in `package-dispatch-readiness.json`. Release asset IDs are not
substituted for these Actions artifact IDs.

## Exact full-package GUI comparison

Use the new completed workflow packages, not the old official release plus a
manually substituted manifest or JavaScript resource. Preserve and authenticate
workflow source SHA, completed/success result, uploaded artifact owner/run/name,
API digest, downloaded ZIP digest, selected package hash and complete extracted
product inventory before running it. Root/package agent owns this intake.

The PWA full-package runner must use a fresh isolated output root, HOME and
profile per source/platform, then install its own two local fixtures through
that package's real UI. Cold CLI commands must be read from the launchers that
same product generated; no old `native-profile`, copied SSB store, manually
patched product copy or replaced omni.ja is used. Compare baseline and PR3032
X11 cold Alpha → Beta → browser identity, plus native Wayland if the existing
nested compositor remains available. Record own PID/profile, class/app_id,
desktop correspondence, sandbox level, rollout state, package hashes before/
after, and normal owned-process shutdown. Actual malformed-record API probes
must use the same unmodified full package and preserve the existing all-empty
random-fallback limitation.

PR3030's Japanese/es-CL language-pack/font-theme matrix is handled by the
branding/full-package agent to avoid duplicate work. Its full-package intake
must satisfy the same source/artifact/inventory requirement; the earlier
manually copied branding manifest evidence is not relabelled as this build.
