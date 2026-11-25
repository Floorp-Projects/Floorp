# FDH platform QA step — live runs (KAN-145)

Date: 2026-11-25. Driven from the Windows dev PC with
`fdh job start --step platform` against the two KAN-144 targets: the stability
tart VM on the Mac Studio (macOS) and a per-task container on the Debian QA host
(Linux). Each job ran `fdh platform-qa run` under the durable executor. The
reports below are the files that run wrote to `_dist/fdh/platform/<task-id>/`;
host addresses are redacted by the runner and do not appear in them.

Payloads (Floorp 12.21.1@159.0.1, the same files as the KAN-144 runs; this
change is tools-only, so they test the QA step, not a product change):

- macOS: `floorp-macOS-universal.dmg`, SHA-256
  `e3520c1f553319ff704a821cbcc72dae49e544459843ba2e8fadaff8ce370629`
- Linux: `floorp-linux-x86_64.tar.xz`, SHA-256
  `584e4afd9865df3a5867b145ed75b7c3051a0a26d173da240056a4b355bfcad9`

## Runs

| File                                          | What it shows                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              |
| --------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `live-macos-report.json`, `live-macos.png`    | Task `kan-145-live`, macOS job started at the same time as the Linux one. All steps passed: payload hash, preflight, lock, upload + install, launch, probe (`Floorp`, `Darwin`, 1 window), `--eval` returned `Floorp 12.21.1@159.0.1` and `--expect Floorp` matched, screenshot, clean cleanup. Outcome `passed`; job `succeeded`, assertion `platform-qa-macos` passed. Screenshot SHA-256 `136647763fcfa4202e2199fe6296524769adb99589861ab1130dde50aee11be8`.                                                            |
| `live-linux-report.json`, `live-linux.png`    | Same task, Linux job, the same steps and eval result in the container (`Linux`, content-sandbox level 6 configured). Outcome `passed`; assertion `platform-qa-linux` passed. Screenshot SHA-256 `1a39a68eae33de8fc24fc9d5d1d669f5e0ca52ceabfe9a37f580308bdec685dd`. `fdh evidence` for the task validated with state `succeeded`, both assertions and the platform QA limitation.                                                                                                                                          |
| `expect-fail-linux-report.json`               | Task `kan-145-fail`, `--expect NotThere`. The run reached eval (value `Floorp`), the expected text was absent: outcome `failed`, reason `eval-expect-not-found`, cleanup still clean. The job ended `failed` (exit 1).                                                                                                                                                                                                                                                                                                     |
| `blocked-linux-report.json`                   | While that job held the container, a second task (`kan-145-other`) ran `platform-qa run` directly: preflight saw the container held by another task, outcome `blocked`, reason `blocked: container-held`, nothing acquired and no cleanup attempted (exit 1).                                                                                                                                                                                                                                                              |
| `resume-job.json`, `resume-linux-report.json` | Task `kan-145-resume`. The executor of attempt 1 was killed (`taskkill /F`) about 12 s after the container was created. `job wait` reconciled: lease expired, the lost step tree was stopped (no process of it was left), attempt 2 (epoch 2) started. The run found the container held by its own task, removed it (`reset`), repeated preflight and passed. The job ended `succeeded` with attempts `lost`, `succeeded`. On Windows the step process did not outlive its executor, so two epochs never ran at once here. |

After the runs the Debian host had no `fdh-qa-*` container and no
`/tmp/fdh-qa-*` directory, and the macOS lock was free.

## Limits

- One machine per platform; no Windows target yet (KAN-147).
- The payload is the operator's input: fdh records its hash and version but not
  which commit built it. These runs used a released build.
- The probe and `--eval` check what the script returns, not how the UI looks;
  the screenshots are stored for a human and are not judged.
- See `docs/qa/fdh-qa-macos/` and `docs/qa/fdh-qa-linux/` for each target's own
  limits (Xvfb without a desktop session or user-namespace sandbox on Linux; the
  shared stability VM on macOS).
- Not QA3, release qualification or final-package QA.
