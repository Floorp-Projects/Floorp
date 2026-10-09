# QA309 — P2-NEW-01 real setup validation

Implemented in `/workspace/qa-review-v030/worktrees/Runtime-shadow-fixes` at Runtime #83 base/head `81c92fc4bbbc13b4fd4c2a2adaf6a137e342bd53`. This agent changed only `qa3_build_profile.py`, `setup-floorp.sh`, and `test_qa3_build_profile.py` under `.github/workflows/scripts`. No docs change, commit, push, native/full build, browser execution, private/paid run, CI dispatch, credentials, signing or publication was performed.

## Final behavior

`setup-floorp.sh` validates argument count and resolves platform, architecture, strict boolean Debug/PGO, PGO mode and artifact through `qa3_build_profile.profile` before exporting a build date, applying Debug patches, writing mozconfig, copying branding, rewriting source or invoking apt/Rust/bootstrap. Both CLI phases receive the same complete inputs. The final CLI enables canonical Debug tests and calls `verify_config` before writing its test override; setup invokes apt/bootstrap only after that assertion succeeds.

`profile` accepts legacy Debug+PGO only with `allow_legacy_debug_pgo=True`. Existing setup callers preserve their experimental behavior by selecting `--allow-legacy-debug-pgo` explicitly when both booleans are true. These profiles keep the `legacy-debug-profile-*` role, `testsOverride=false` and `qualificationEligible=false`; the input-validation phase prints a noncanonical/ineligible warning. This is an explicit compatibility policy, not canonical qualification.

The final assertion verifies generated target, Debug/profiling flags, canonical Debug tests and profile-use LTO/profile/jarlog options. It remains a generated-config check: it does not claim configure execution, usable profile bytes or native test execution. Production templates and workflow callers were not changed.

## Red regressions preserved before implementation

- [setup-red-first.stderr.log](setup-red-first.stderr.log), [stdout](setup-red-first.stdout.log), [index](setup-red-first.json): all original six invalid inputs were accepted with exit 0. Raw failure messages contain the patch/apt/Rust/bootstrap stub calls. Production-file hashes were unchanged from #83 when this red test ran.
- [setup-red-config-first.stderr.log](setup-red-config-first.stderr.log), [index](setup-red-config-first.json): four generated-config regressions. Three corrupt configurations exited 0; the ambiguous Debug tests configuration already exited nonzero but reached apt before rejection. The wrapper initially expected three failures; all four raw observed failures were preserved and were not rerun or overwritten before the fix.

## Final verification

| Check | Outcome | Evidence |
|---|---|---|
| Build-profile module | 12 test methods PASS | [setup-final.stderr.log](setup-final.stderr.log), [run/index with final hashes](setup-final.json) |
| Original real setup invalid inputs | 6 rejected before all observed effects | `test_real_setup_rejects_invalid_inputs_before_side_effects` |
| Additional input/count negatives | 7 incomplete/invalid inputs + 2 argument-count cases rejected | `test_real_setup_rejects_invalid_booleans_and_incomplete_modes`, `test_real_setup_argument_count_and_optional_defaults` |
| Generated config negatives | 4 rejected before apt/Rust/bootstrap | `test_real_setup_rejects_wrong_generated_config_before_bootstrap` |
| Valid target/mode matrix | 5 canonical Debug + 13 production opt/generate/use + 8 legacy Debug+PGO cases PASS | Build-profile matrix tests |
| Exact backward config compatibility | 26/26 successful configs byte-identical to fixed #83 | [setup-compatibility-bounded-pack.json](setup-compatibility-bounded-pack.json) |
| Adjacent source-cohort contracts | 14 test methods PASS | [setup-adjacent-first.stderr.log](setup-adjacent-first.stderr.log), [index](setup-adjacent-first.json) |
| Ruff 0.16.10 check and format, repo pyproject rules | PASS | [setup-static-final.json](setup-static-final.json), [check](setup-ruff-final.stdout.log), [format](setup-format-final.stdout.log) |
| Bash syntax | PASS | [setup-bash-final.stderr.log](setup-bash-final.stderr.log) |
| Scoped git diff whitespace | PASS with per-process preloadIndex disabled | [setup-diff-single-thread-final.json](setup-diff-single-thread-final.json) |

The real setup harness copies only the scripts/templates into its own temporary tree, uses recording exit-0 stubs for git/sudo/rustup/mach, and supplies a fresh environment without inherited credentials. Invalid-input tests also require absent mozconfig, no copied branding and unchanged updater input. Executed checks used a 256 MiB address-space bound, CPU 20 seconds, file-size 8 MiB, core dumps disabled and wall timeouts. No real bootstrap/build ran.

## Tooling limits and attempt preservation

Mozlint was invoked for the three changed files as required by the repository skill, but did not reach code linting. [First raw attempt](setup-mozlint-first.stderr.log) hit the read-only default uv cache. Subsequent attempts used only an owned writable state/cache, disabled package downloads and exposed an invalid system-package-source override, then the actual offline dependency block: `aiohappyeyeballs==2.6.1` and optional packages were not cached. [Final raw blocker](setup-mozlint-offline-fourth.stderr.log), [index](setup-mozlint-offline-fourth.json). No further environment bootstrap/download was attempted. Shared Ruff was used directly; shellcheck was absent from PATH and the narrowly searched tooling directory. Bash syntax passed, but shellcheck is not claimed.

One recording-wrapper path construction error occurred after an initial standalone Ruff/format and second mozlint invocation. The wrapper dropped their captured output; this gap is explicitly recorded in [setup-recorder-first-error.log](setup-recorder-first-error.log). Sources were unchanged before the recorded retries. The recorded Ruff failure (`PLW1510`, missing explicit `check=False`) was fixed and the final checks passed. Original functional red logs were not affected.

Under the 256 MiB fixture limit, Git's default threaded lstat and a packed-object mapping failed. [Raw lstat error](setup-diff-final.stderr.log), [raw packed-source error](setup-compatibility-source-read-failure.json), [first comparison-wrapper trace](setup-compatibility-first-error.log). Only per-process Git settings were changed for the successful retries: `core.preloadIndex=false`, `core.packedGitWindowSize=8m`, `core.packedGitLimit=32m`, `core.deltaBaseCacheLimit=8m`. Repository config was not modified. These tooling failures are not implementation failures.

## Frozen handoff

[setup-final-files.json](setup-final-files.json) records final file SHA256/size/mode and [setup-implementation.patch](setup-implementation.patch), whose SHA256 is `2bf173efed01071e4e1c7ce2eb0731c861c7a43c7ab64dd5f8736bf6620dd4eb`.

| File | SHA256 |
|---|---|
| qa3_build_profile.py | `e7f84f39d36355074e15e3d192249cd5adde954110fc5f820b2bda6c47dbc00a` |
| setup-floorp.sh | `9bf55ac325397d76d3d162e681ba2bc47facc8b2ab61e68c2646e2d1b95f651c` |
| test_qa3_build_profile.py | `5b8452881d0107ea3f19643e1d36d380fd718753b9b9e51e440dc5642ded9c96` |

Ready for the requested independent setup reviews and root integration into #83/#84. No native qualification or publish readiness is inferred from these local checks.
