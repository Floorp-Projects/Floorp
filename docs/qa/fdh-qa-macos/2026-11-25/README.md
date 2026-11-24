# FDH macOS QA target — live runs (KAN-144 step 1)

Date: 2026-11-25 (guest clock UTC 2026-11-24). Target: the long-lived macOS
stability VM (tart, macOS 26.6.2) on the Mac Studio host, driven from the
Windows dev PC with `tools/src/fdh_qa_macos.ts` and the live scripts in
`tools/integration/`. Payload: Floorp 12.21.1@159.0.1 as
`floorp-macOS-universal.dmg` (SHA-256
`e3520c1f553319ff704a821cbcc72dae49e544459843ba2e8fadaff8ce370629`) and as a
`.tar.gz` of the same `Floorp.app`, built from that dmg on the host (SHA-256
`707862b7b8608cf0de2147fd97bc62687c3950af49aa5217abcdd46f78256a2f`).

Guest keys stay on the Mac host. The operator side used only its existing SSH
access to the host; the config names the key files inside the VM directory on
the host. Host and guest addresses are not part of these reports.

## Runs at the code in this PR (after the review fixes)

| File                         | Script                      | What it shows                                                                                                                                                                                                                                        |
| ---------------------------- | --------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `kan-144-live-5-report.json` | `fdh-qa-macos-live.ts`      | dmg: ready preflight, lock, upload (260 MB) + hash check + install, launch in the GUI session with a fresh profile, Marionette probe (`Floorp`, `Darwin`, 1 window), screenshot, cleanup with 0 residual processes, ready target afterwards. Exit 0. |
| `kan-144-live-6-report.json` | `fdh-qa-macos-live.ts`      | Same cycle with the `.tar.gz` payload (237 MB). Exit 0. Its screenshot was byte-identical to run 5's (same SHA-256 in both reports), so only run 5's PNG is kept.                                                                                    |
| `kan-144-live-5-macos.png`   | —                           | Screenshot from run 5 (Marionette `WebDriver:TakeScreenshot`, chrome context). SHA-256 `136647763fcfa4202e2199fe6296524769adb99589861ab1130dde50aee11be8`.                                                                                           |
| `lock-check.json`            | `fdh-qa-macos-lock-live.ts` | Lock: task A acquires and re-acquires as a no-op; task B is refused (`lock-held`) for acquire and cleanup; preflight shows A as holder; A's cleanup releases it. `passed: true`, exit 0.                                                             |

## Earlier revision

| File                         | Revision  | What it shows                                                                                                                                                                                 |
| ---------------------------- | --------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `kan-144-live-3-report.json` | `d30d510` | macOS had reopened an older Floorp at login: preflight counts `foreignFloorp: 1` and the run refuses with `foreign-floorp-running` before taking the lock. Not re-run: the condition is gone. |

A run without a GUI session (launch refused with `gui-session-absent`, clean
cleanup) was also made on the first revision; it is not kept because that
revision proceeded past a not-ready preflight.

## Limits

- Reset is application-level: the task directory (app copy and profile) and
  every process under it are removed. Not reset: OS settings, TCC grants, the
  test user's `~/Library/Application Support/Floorp`, crash reports, Saved
  Application State and caches created by the login session.
- One task per target at a time (root-owned symlink lock in
  `/private/var/db/fdh-qa`).
- Not QA qualification, release evidence or a QA3 replacement.

## Guest prerequisites

- A test user (`floorptest`) logged in to the GUI; the guest SSH account
  (`admin`) has passwordless sudo.
- python3 at `/usr/local/bin/python3` or the python.org framework path, for the
  Marionette client.
- Done by hand for these runs: the test user was logged in over VNC by the
  operator; `TALLogoutSavesState` and `LoginwindowLaunchesRelaunchApps` were set
  to false for the test user, and the old Floorp it reopened at login was quit.
