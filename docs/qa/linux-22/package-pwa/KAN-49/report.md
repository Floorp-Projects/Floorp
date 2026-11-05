# Linux PWA window identity fix validation

Base Floorp main: `984509d2053be7bae9027133bda7259cd706b208`.
Independent worktree `/workspace/qa-linux-pwa-class-fix`, branch
`codex/linux-pwa-window-class`. Changes are limited to three files.

## Defect and correction

Real UI installs generated unique desktop Exec `--name` and StartupWMClass for
each PWA, but all live windows advertised `WM_CLASS="Navigator", "floorp"`
in three launch orders. The packaged Gecko browser-init.js reads the existing
`taskbartabclass` creation option when a `taskbartab` id is present and applies it
to the native `windowclass` attribute. Floorp passed its SSB id as taskbartab but
did not pass the class option. A process-level `--name` is not sufficient to
identify later windows independently.

The correction passes the generated launcher class via that existing Gecko
option on Linux. LinuxSupport exposes one shared class derivation for launchers
and window creation, retaining the prior normalization. No taskbar experiment
predicate, enrollment, native-service discovery, icon-setting code, Runtime
source, preference, default or platform other than Linux is changed.

An initial candidate used an unconsumed `windowclass` property-bag key and failed
all observed class checks; this failure is retained under
`pwa-order-candidate-unconsumed-option`. The final candidate uses Gecko's actual
supported `taskbartabclass` key. It is not a forced experiment success.

## Actual product comparison

Official Floorp `12.21.1@159.0.1`, BuildID `20261103153232`, Debian 13,
Xvfb X11, isolated HOME/profile, session D-Bus, synthetic CLI startup.
The candidate is an isolated copy of this product with **two JavaScript resources
changed in omni.ja**, not a full source build or signed release. All 45 other
product files were verified byte-identical, including native binaries. Provenance
is recorded in `pwa-class-candidate-provenance.json`; the rebuild script refuses
unexpected source selectors or additional product file changes.

| Launch order | Unchanged official product | Candidate |
| --- | --- | --- |
| Alpha → Beta → browser | both PWA classes shared `floorp` | each PWA matches its generated desktop class; browser `floorp` |
| Beta → Alpha → browser | both PWA classes shared `floorp` | each PWA matches its generated desktop class; browser `floorp` |
| browser → Alpha → Beta | both PWA classes shared `floorp` | each PWA matches its generated desktop class; browser `floorp` |

All six measured runs retain **content sandbox level 6** and **Linux taskbar
experiment false**; all exit through normal Marionette Quit (`forced:false`).
Final measurements use dedicated automation port 2839, exact ProfD matching
before control/quit, and own product PID matching for X11 windows. Raw whole
display trees can contain other concurrent QA windows; those are excluded from
the machine-readable class comparison.

Evidence: `pwa-order-run/result.json`, `pwa-order-candidate/result.json`, their
per-window X11 properties, original generated desktop entries/icon hashes,
`pwa_order_probe.py`, and `make_pwa_class_candidate.py`.

## Regression test

The new colocated browser test opens two actual PWA windows via the real
SsbRunnerUtils, checks exact UUID-derived launcher classes, checks their
distinctness, and asserts the existing rollout result does not change. It closes
its owned windows in `finally`.

The exact new test body plus existing assertion harness was bundled and executed
inside each real packaged product through supported Marionette chrome system
automation. It **fails** on baseline (`windowclass` actual null) and **passes**
on the candidate. Both runs retain sandbox 6 and terminate normally. This is a
focused packaged-product runner, **not the standard colocated runner** and not
execution of the separate existing container-navigation test.

Evidence: `regression-baseline.json`, `regression-candidate.json`,
`build_pwa_regression.cjs`, `run_pwa_regression.py`, and generated
`pwa-colocated-regression.js`. The first file-loader harness attempt was rejected
as an untrusted file URI before executing the test; that setup failure is retained
as `regression-fileloader-harness-failure.json`, not a product failure.

Scoped Deno typecheck, lint, format and git diff checks pass for all three changed
files. Typecheck used the existing read-only dependency installation via a local
worktree symlink; no shared dependency settings were changed.

## Remaining review conditions

KDE taskbar icon behavior and native Wayland app_id remain separately measured
under KAN-48. This X11 correction alone does not establish the original
Manjaro/KDE version or Outlook/Teams startup conditions. Full production source build remains required; final-head standard runner/CI is verified in the final section below. Review the
new import dependency at early Linux command-line startup and malformed legacy
SSB records; valid UUID fixture launchers and all tested creation orders pass.
Draft PR only; no merge, release or Done transition is authorized.

## Follow-up at final head8249e234

Read followup-validation.md for the additional cold command-line, malformed-data and native Wayland evidence. Follow-up found a real numeric/object-ID startup regression in initial candidate06aaf589 and corrected it in8249e234 with a string-type guard plus additional colocated coverage. All9 API cases open in the final candidate; wholly unusable records retain the pre-existing random-identity limit. Native Wayland UI and cold generated-launcher tests preserve distinct PWA app_id/desktop/KWin identity. This does not qualify the separate taskbar icon pixel condition. Prior-head CI success remains historical; new-head run42050337102 is checked separately.

## Final-head standard CI

Head8249e234 run42050337102 is successful. Ubuntu artifact13427512330 was downloaded:110385bytes, SHA2569c825202b8265319195d5e168e9d004db2a21468f1c4000508edd60dde3c84b8 matches GitHub metadata. Standard runner records SsbCommandLineHandler.runAllTests7341ms,281 browser modules passed/0 failed and8 downloaded Firefox files/16 tasks passed. Attempt1 Mac npm dependency body-read failure occurred before product tests and is preserved; a single same-head specific-job retry after run completion succeeded. Previous head06aaf589 CI is historical. See ../ci/pr3032-final/.


## Full production package follow-up

Exact final PR3032 8249e234 and baseline984509d2 were built by the existing full production application package workflow with the same unchanged authenticated prebuilt Runtime. Both CI runs succeeded. Baseline X11 has common floorp classes; baseline native Wayland makes Beta inherit Alpha app_id. Full candidate fixes both PWA launcher identities on both protocols. Four fresh UI-install/cold-launch runs complete, all36legacy API windows open, normal quit, sandbox6, rolloutfalse, product inventories unchanged. Ordinary browser after PWA still inherits Alpha native Wayland identity in both baseline/candidate; this pre-existing condition is retained, not silently marked fixed. Original Manjaro/KDE/Outlook/Teams and all-invalid legacy identity limitations remain. See `../full-build/report.md` and full-build per-product result JSON.

Jira full-package evidence comment: 10353. Status remains in progress; no Done transition.
