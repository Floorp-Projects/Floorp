# FDH macOS QA target — live runs (KAN-144 step 1)

Date: 2026-11-25 (guest clock UTC 2026-11-24 19:40–19:50). Target: the
long-lived macOS stability VM (tart, macOS 26.6.2) on the Mac Studio host,
driven from the Windows dev PC with `tools/integration/fdh-qa-macos-live.ts` and
`tools/src/fdh_qa_macos.ts`. Payload: `floorp-macOS-universal.dmg` (Floorp
12.21.1@159.0.1), SHA-256
`e3520c1f553319ff704a821cbcc72dae49e544459843ba2e8fadaff8ce370629`.

Guest keys stay on the Mac host. The operator side used only its existing SSH
access to the host; the config names the key files inside the VM directory on
the host. Host and guest addresses are not part of these reports.

## Runs

| File                         | What it shows                                                                                                                                                                                                                                                                   |
| ---------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `kan-144-live-1-report.json` | No GUI session: lock, upload (260 MB), hash check and install succeed; launch refuses with `gui-session-absent`; cleanup leaves 0 processes, removes the task directory and releases the lock.                                                                                  |
| `kan-144-live-3-report.json` | macOS had reopened an older Floorp at login: preflight counts `foreignFloorp: 1` and the run refuses with `foreign-floorp-running` before taking the lock.                                                                                                                      |
| `kan-144-live-4-report.json` | Full cycle after the old Floorp was quit and login-window relaunch was disabled for the test user: ready preflight, stage, launch in the GUI session with a fresh profile, Marionette probe (`Floorp`, `Darwin`, 1 window), screenshot, clean cleanup, ready target afterwards. |
| `kan-144-live-4-macos.png`   | The screenshot from run 4 (Marionette `WebDriver:TakeScreenshot`, chrome context). SHA-256 `136647763fcfa4202e2199fe6296524769adb99589861ab1130dde50aee11be8`, as recorded in the report.                                                                                       |
| `lock-check.json`            | Per-target lock: task A acquires, re-acquires as a no-op; task B is refused (`lock-held`) for acquire and cleanup; preflight reports `lock-held` while A holds it; A's cleanup releases it.                                                                                     |

## Limits

- Reset is application-level: the task directory (app copy and profile) and
  every process that references it are removed. OS settings, TCC grants and
  macOS caches created by the login session are not reset.
- One target, one task at a time (mkdir lock in the guest).
- Not QA qualification, release evidence or a QA3 replacement.
- Guest preparation done by hand for these runs: the test user was logged in
  over VNC by the operator; `TALLogoutSavesState` and
  `LoginwindowLaunchesRelaunchApps` were set to false for the test user, and the
  old Floorp it reopened at login was quit.
