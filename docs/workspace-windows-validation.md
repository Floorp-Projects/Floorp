# Windows workspace validation

These instructions run the current Floorp sources in the locked native Windows
browser. They do not rebuild Gecko or add Windows three-finger input. See
[workspace sessions](workspace-sessions.md) for the feature behavior and
[native input support](workspace-native-input.md) for the native build boundary.

## Prerequisites and source identity

Use Windows x64 with Git for Windows, Deno 2.x, and Node.js 22 on `PATH`. Run the
commands below in PowerShell from the repository root. A short checkout path
helps with Windows path limits; enable Git long paths before cloning:

```powershell
git config --global core.longpaths true
$repoDir = Join-Path $env:USERPROFILE "src\Floorp"
git clone https://github.com/Floorp-Projects/Floorp.git $repoDir
Set-Location $repoDir
```

Check out the revision being validated, then record its identity and tool
versions. Replace the quoted placeholder with the actual commit:

```powershell
git checkout --detach "COMMIT_TO_VALIDATE"
git rev-parse HEAD
git status --short
git --version
deno --version
node --version
```

For an existing checkout, use its current sources and record any local changes.
Hosted CI validates the published commit, so uncommitted local changes are not
covered by a remote run. The local toolchain creates directory junctions when
Windows does not permit directory symlinks; Developer Mode is not required for
that fallback.

## Install the locked Windows Runtime

```powershell
deno install
if ($LASTEXITCODE -ne 0) { throw "Dependency installation failed" }
deno run -A tools/runtime-lock/runtime_lock_cli.ts validate-lock
if ($LASTEXITCODE -ne 0) { throw "Runtime lock validation failed" }
deno run -A tools/runtime-lock/runtime_lock_cli.ts install-native
if ($LASTEXITCODE -ne 0) { throw "Windows Runtime installation failed" }
```

`install-native` selects the Windows x86_64 ZIP from
`floorp-runtime.lock.json`, checks its digest, and installs
`_dist/bin/floorp/floorp.exe`. The test startup applies the repository's artifact
patches and builds the current TypeScript sources. Installing the Runtime alone
does not execute any browser tests.

## Automated browser tests with native Windows widgets

Close a previously launched test or development browser before starting. Each
scoped run writes its filter before browser startup, starts a fresh browser, and
verifies teardown. The test profile is `_dist/profile/test`, separate from an
installed Floorp profile.

Use a normal Windows desktop browser window for the complete 49-file suite,
with `MOZ_HEADLESS` unset. The dedicated CI matrix also uses native Windows
widgets so Windows APIs receive real native window handles. UI fixtures also
load a bookmarkable page before opening StarUI; a fresh blank/home page can
hide the native URL-bar anchors independently of the rendering mode.

The runner takes its options directly after `deno task test`; do **not** insert
an extra `--` separator:

```powershell
Remove-Item Env:MOZ_HEADLESS -ErrorAction SilentlyContinue
deno task test --near browser-features/chrome/common/workspace-gestures --timeout-ms 300000 --startup-timeout-ms 180000
if ($LASTEXITCODE -ne 0) { throw "Workspace gesture tests failed" }
```

The dedicated Windows workflow runs these eight scopes as separate matrix jobs,
with up to four jobs in parallel. Each job installs a fresh native Runtime and
uses its own test profile. One failed suite does not cancel the others. The
counts are browser test files, each of which can contain multiple assertions or
tasks.

| Scope | Expected passing files |
| --- | ---: |
| `browser-features/chrome/common/workspaces` | 24 |
| `browser-features/chrome/common/workspace-gestures` | 3 |
| `browser-features/chrome/common/workspace-session-indicator` | 2 |
| `browser-features/chrome/common/workspace-bookmarks` | 2 |
| `browser-features/chrome/common/command-palette` | 9 |
| `browser-features/chrome/static/overrides/modules/workspaces` | 2 |
| `browser-features/pages-modal-child` | 2 |
| `browser-features/modules/modules/test` | 5 |
| **Total** | **49** |

To reproduce the same coverage serially on a local machine:

```powershell
Remove-Item Env:MOZ_HEADLESS -ErrorAction SilentlyContinue
$scopes = @(
  "browser-features/chrome/common/workspaces",
  "browser-features/chrome/common/workspace-gestures",
  "browser-features/chrome/common/workspace-session-indicator",
  "browser-features/chrome/common/workspace-bookmarks",
  "browser-features/chrome/common/command-palette",
  "browser-features/chrome/static/overrides/modules/workspaces",
  "browser-features/pages-modal-child",
  "browser-features/modules/modules/test"
)
foreach ($scope in $scopes) {
  deno task test --near $scope --timeout-ms 300000 --startup-timeout-ms 180000
  if ($LASTEXITCODE -ne 0) { throw "Browser suite failed: $scope" }
}
```

The suite exercises real Windows browser APIs for workspace/session routing,
container storage isolation and sharing, session recovery, bookmark visibility
and editing, UI labels, palette integration, and the common gesture controller
and end-stop actions. It also measures real Places queries with 100 and 10,000
bookmarks. Automated native-widget coverage does not verify a physical
touchpad, OS gesture delivery, or Windows native three-finger input.

For a limited controller or storage check that does not depend on native UI
geometry, headless mode is optional. For example, this selects only the pure
gesture controller test file:

```powershell
$env:MOZ_HEADLESS = "1"
deno task test --near browser-features/chrome/common/workspace-gestures/test/controller.test.ts --timeout-ms 300000 --startup-timeout-ms 180000
$controllerExit = $LASTEXITCODE
Remove-Item Env:MOZ_HEADLESS -ErrorAction SilentlyContinue
if ($controllerExit -ne 0) { throw "Headless controller check failed" }
```

This limited result does not establish native popup, dialog, label geometry, or
complete 49-file UI coverage. Clear `MOZ_HEADLESS` before returning to the full
suite or physical checks.

Local runner logs are in `logs/test/colocated-*.log`; `latest.log` is replaced
after each run. The Places measurement is written to
`_dist/profile/test/workspace-bookmarks-benchmark.json` by the modules suite.

## Run the dedicated GitHub Actions job

Publish the source revision on a validation branch. In GitHub Actions, select
**(CI) Browser Integration + Runner Tests**, select that branch, and enable
`windows_workspaces_only`. The workflow name also contains a test-tube emoji.
The CLI equivalent is:

```powershell
$validationBranch = "VALIDATION_BRANCH"
gh workflow run colocated_runner_test.yml --repo Floorp-Projects/Floorp --ref $validationBranch -f windows_workspaces_only=true
gh run list --repo Floorp-Projects/Floorp --workflow colocated_runner_test.yml --branch $validationBranch --event workflow_dispatch --limit 5
```

This opt-in dispatch runs the `windows-workspaces` matrix on `windows-latest`
with standard native Windows widgets and skips the normal smoke/native matrix.
Each job name includes its suite, so completed suite logs and artifacts can be
inspected while other jobs are still running. The default dispatch,
pull-request, and main-branch workflows retain their existing behavior. The
ordinary Windows native matrix entry validates the Runtime artifact; it does
not run these browser suites.

Check the run's `headSha` against the intended revision. Each job requires its
scope's exact passing-file count, a successful runner exit, and no skipped files.
All eight jobs must succeed for the complete 49-file coverage. Tests do not run
when that job's Runtime setup fails or the run is cancelled. Download the
`windows-workspace-tests-<suite>-<run_attempt>` artifact for each completed job.
Each contains:

- `_dist/windows-workspaces/source-commit.txt` and a `results.json` array for
  that job's suite;
- the suite's `result.json`, runner log, collected result log, and available
  profile evidence;
- timestamped browser runner logs and the available Vite logs;
- `logs/test/diagnostics-<run-id>.log` when a browser collection failure triggers
  a diagnostic attempt;
- Windows CI desktop PNG/JSON, thread wait information, and available native
  debugger logs when a test failure leaves the browser unresponsive;
- profile preferences and the Places benchmark report, when produced.

Inspect startup and teardown errors separately from browser assertions. A
message that `feles-build test` exited early is not a passing browser run. Its
launcher stdout/stderr are retained on Windows and in GitHub Actions, appearing
in the step output and the suite's `runner.log`. Other platforms' local runs
remain quiet. If result collection fails for an automatically started browser,
the runner attempts to record window state, current test progress, and recent
browser console messages before teardown. The Marionette diagnostic has a
20-second stop timer. Windows CI also attempts a desktop and native thread
capture with a 45-second limit; the screenshot is saved before querying the
unresponsive process. Native debugger output depends on the tools installed on
the runner. An unresponsive browser may yield an error instead of a complete
snapshot. A rejected artifact patch can leave partially modified Runtime files
in that job, so use a fresh hosted run when retrying the failure. Matrix jobs are
isolated; a partial patch cannot carry over into another suite's Runtime.

The Windows runner verifies executable paths and full-precision UTC process
creation times before terminating its browser processes. If an ordinary content
process exits and Windows reuses its PID, the replacement is excluded from
ownership and tree termination is disabled. Changes to the launcher or listener
identity, or unreadable identity evidence, still fail validation.

### Isolate one workspace test file

For a diagnostic run, keep `windows_workspaces_only=true` and set
`windows_workspace_test` to a filename or repository-relative path directly
under `browser-features/chrome/common/workspaces/test/`. For example:

```powershell
$testFile = "browser-features/chrome/common/workspaces/test/nativeLastTabDisabledKeepOpen.test.ts"
gh workflow run colocated_runner_test.yml --repo Floorp-Projects/Floorp --ref $validationBranch -f windows_workspaces_only=true -f "windows_workspace_test=$testFile"
```

With this nonempty input, the matrix contains only `workspaces`, the helper runs
`--near` that file, and exactly one passing browser test file is required. The
helper rejects missing files, nested paths, paths outside the workspace test
directory, symbolic links, and files without the `.test.ts` suffix. The input is
passed through an environment variable rather than inserted into PowerShell
source. The artifact's `near`, `selectedTestFile`, and `expectedFiles` identify
the restricted coverage.

To reproduce this diagnostic locally with native Windows widgets:

```powershell
Remove-Item Env:MOZ_HEADLESS -ErrorAction SilentlyContinue
./tools/run_windows_workspace_suite.ps1 -Suite workspaces -TestFile "$testFile"
```

An isolated pass does not establish the complete 24-file workspace suite or the
49-file matrix. Compare it with the normal batch when investigating effects
from earlier tests. Leave `windows_workspace_test` empty to retain the default
eight-suite, 49-file matrix; ordinary CI behavior is unchanged.

## Physical Windows touchpad checks

Physical Windows touchpad verification remains pending.

Record the Windows version/build, display scale, touchpad model, driver/version,
Precision Touchpad status, configured OS gestures, and exact tested commit. For
manual testing, close the automated test browser and launch a visible instance
using the same locked Runtime:

```powershell
Remove-Item Env:MOZ_HEADLESS -ErrorAction SilentlyContinue
$env:FLOORP_DEV_RUNTIME_DISTRIBUTION = "release"
deno task feles-build dev
```

Windows direct native three-finger input is **not implemented**. Its workspace
input settings must report that limitation and must not offer a working native
mode. Setting native preferences does not add Windows support. Do not record an
OS-generated key chord as a native three-finger event.

For an OS gesture-to-key test, keep Floorp's sidebar swipe mode off first. In
Floorp's keyboard shortcut settings, assign distinct unused chords to:

- **Next Workspace (Stop at End)** — `floorp-workspace-next-no-wrap`;
- **Previous Workspace (Stop at Start)** — `floorp-workspace-previous-no-wrap`.

Where supported, map the touchpad's horizontal three-finger directions to those
same chords. Windows 11 normally exposes touchpad settings under **Bluetooth &
devices → Touchpad → Advanced gestures**; older Windows versions and driver
utilities can use different paths or omit custom shortcuts. Record unsupported
mapping as unavailable on that device. Floorp does not configure the OS mapper
and cannot recover physical gesture boundaries from a repeated key stream.

- [ ] Create workspaces A, B, and C with distinct selected tabs. Verify the two
  assigned keyboard chords before testing the mapper.
- [ ] From B, make one deliberate horizontal gesture in each mapped direction.
  Confirm exactly one switch and preservation of each workspace's selected tab.
- [ ] At A and C, repeat the outward gesture. Selection stays at that end; it
  never wraps. Test repeated gestures and a held/repeated key separately.
- [ ] Try slow/fast movements, release/coasting, reversal, vertical movement,
  and a cancelled movement. Record which inputs cause the mapper to send a
  chord. Configure one key press per completed intended gesture; duplicate
  switches are a failure of this mapping setup.
- [ ] Test two Floorp windows with different selected workspaces. Only the
  foreground target window should switch. Check the location bar and a page
  text field for shortcut conflicts, and retain normal OS behavior when another
  application is foreground.
- [ ] Confirm two-finger page scrolling, history gestures, pinch zoom, and tab
  dragging retain their expected behavior. Check conflicts with desktop/task
  switching gestures configured by Windows or the driver.
- [ ] Inspect normal, pinned, collapsed vertical, and split-view session labels
  on the visible desktop at the tested display scale; check the pending-session
  recovery button and keyboard focus. Automated native-widget checks do not
  replace this visual check.
- [ ] If testing optional sidebar horizontal swipe, disable the OS mapping for
  that check and enable only sidebar mode. Verify one switch, stopping at ends,
  and that page content/form controls do not become workspace swipe targets.
  This wheel-based path does not establish a finger count.

Restore the tester's original gesture mappings after recording the results.

## Results

Pending. Add the tested commit, GitHub Actions run URL, per-suite outcomes,
Windows Runtime version/build ID, benchmark evidence, and any failures after
the remote run completes. Record physical-device results separately; an
automated browser success must not mark those checks complete.
