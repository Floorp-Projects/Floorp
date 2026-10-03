# macOS Web App Shim

This implementation adds a native, app-specific macOS process while keeping
Gecko, profiles, cookies, and site storage in the existing Floorp parent
process. It is experimental and disabled by default. The native backend is
included in the pinned Runtime `daily-1117`. Runtime artifacts from before the
native backend was merged cannot provide it.

## Ownership

```text
Floorp parent / existing profile
  App registry, permissions, cookies, site storage, navigation, quit policy
  Gecko content processes and WebRender
        |
        | Authenticated Mach messages + IOSurface rights
        v
App-specific signed .app / App Shim process
  NSApplication, menus, Dock identity, NSWindow, NSView, NSTextInputClient
```

The App Shim does not open a profile or embed a second browser engine. The
browser creates `MacWebAppWidget` instances for the app's chrome windows, with
the same browser contexts and container IDs used by ordinary Floorp. Related
windows inherit their app identity from their parent or opener. The Shim owns
the actual WindowServer windows; the browser does not create hidden proxy
NSWindows for them.

WebRender composites into a BGRA IOSurface using its existing full-window
compositor path. The surface is transferred without a screenshot or CPU
readback. Per-window native overlay composition is disabled for these widgets,
so unsupported YUV video overlay surfaces are not exported. HDR and protected
video require separate validation and support.

The macOS compositor options must propagate the Widget's native-compositing
policy, and the reported WebRender compositor must honor the same option.
`macos-app-shim-pwa-input-and-compositor.patch` carries this native fix. The Shim
accepts int32 world coordinates and translations before applying the viewport
transform, while preserving surface dimensions and memory limits.

Phased trackpad input uses Gecko's pan/APZ, overscroll, and history-swipe handling,
including the system swipe setting and content's horizontal scrollability.
Phase-less wheel input keeps its wheel path. The IME client maintains the
composition start across updates and applies selection offsets relative to that
start.

AppKit popup menus need an NSView in the process displaying the menu. PWA widgets
have no host-process NSView, so `NativeMenuSupport::CreateNativePopupMenu` leaves
their popups to Gecko and the Shim's native popup windows. This includes select
dropdowns and context menus. Ordinary browser widgets retain their native menu
policy; neither native-menu preference needs to be disabled.

## Launch and installation

The existing installation ID and bundle identifier are retained. The app
registry adds a stable profile ID and the signed bundle's identity. A candidate
bundle is built and signed beside the installed app; its path, full fingerprint,
and transaction ID are journaled. The candidate must authenticate, create its
native window, and acknowledge a real Gecko frame before the installer exchanges
the bundles atomically. Recovery compares actual fingerprints before rollback or
removal. An unrelated replacement at the same path is never deleted.

Finder startup verifies the sealed host identity, launches the normal Floorp
executable with the existing profile and app ID, then exits. Standard profile
remoting forwards requests to an existing host. The host starts the persistent
Shim with a fresh one-time token; both sides verify the sender's Mach audit
token and code identity. Subsequent packets require the same process generation
and strictly increasing sequence numbers.

Restart/reset profile environment variables and remoting overrides are removed
from the temporary launch, so an inherited environment cannot replace the
profile sealed into the installed app.

Rename preserves the app's path and bundle ID and re-signs its metadata.
Native installation metadata lives under `Contents/Resources/floorp.json`, so
macOS seals it as a resource rather than treating it as unsigned nested code.
Uninstall waits for that app's windows and process to close before retiring its
verified bundle. Container changes for native installations are rejected so the
installed app does not silently switch its login state. Reinstallation is the
explicit way to choose another container.

A committed installation currently reuses its recorded Shim generation. It does
not compare or replace that executable when a newer browser ships a newer Shim;
automatic release refresh is not implemented. A future update must verify the
old receipt, stage the new binary at the same app identity, and validate its
connection and first frame before exchanging bundles. Re-signing display metadata
does not perform this binary update.

The host requirement is sealed into each app. A production signing requirement
may remain valid across browser updates, but an ad-hoc development requirement
can pin the previous browser's code hash. After rebuilding or re-signing that
host, an existing Shim can reject cold startup, and current-host bundle
verification can reject reuse. The implementation fails closed rather than
silently weakening that requirement. Receipt-verified host trust refresh and
browser relocation are additional release requirements.

## Quit behavior

`floorp.browser.nativeApp.keepRunningAfterBrowserQuit` defaults to `true`:

| Action                                 | Result                                                                        |
| -------------------------------------- | ----------------------------------------------------------------------------- |
| App's Cmd+Q                            | Close that app's windows, honoring beforeunload, then terminate its Shim      |
| Floorp Cmd+Q, setting enabled          | Close ordinary browser windows and retain the shared parent while apps remain |
| Floorp Cmd+Q, setting disabled         | Use normal global browser shutdown                                            |
| Browser restart or OS shutdown         | Use global shutdown; do not convert it into browser-only close                |
| Last app exits after browser-only quit | Release the now-idle shared runtime                                           |

The settings control is available only when the native lifecycle adapter reports
support. Browser-owned legacy Web App windows keep their previous behavior.

## Build

The pinned Runtime `daily-1117` already contains the Shim and host sources.
Its build graph produces `floorp-app-shim`, registers `nsIMacWebAppService`,
and includes the Shim in the macOS package manifest. Use the exact
`source.commit` in `floorp-runtime.lock.json` for source builds, then build and
package the matching Floorp frontend. Native changes require a newly built and
published Runtime artifact; `package.yml` cannot recompile a prebuilt artifact.

Reference sources under `native/macos-app-shim/` provide the isolated native
build and tests without a Gecko checkout. Keep them aligned with the Runtime
implementation.

For repeated local builds, stage into a fresh packaging directory before signing.
Signing changes file timestamps; an older signed staged executable can otherwise
appear newer than its replacement to an incremental package copier. Verify the
staged Shim against the current build before running integration tests.

For an isolated native-only build and test:

```sh
deno task app-shim:build --test
deno task app-shim:build --universal --test
deno task app-shim:test
```

The universal option requires the relevant SDK/toolchain support. A local
architecture build is not evidence that both architectures have been tested.

Enable `floorp.browser.nativeApp.appShim.enabled` only in a disposable test
profile using the matching source-built Runtime. The default remains `false`.

## Validation

After building and signing the local browser `.app`:

```sh
python3 tools/app-shim/test-runtime.py --browser /absolute/path/to/Nightly.app --launch-services --capture-window
python3 tools/app-shim/test-browser-modules.py --browser /absolute/path/to/Nightly.app
python3 tools/app-shim/test-popups-and-overscroll.py --browser /absolute/path/to/Nightly.app
python3 tools/app-shim/test-runtime.py --browser /absolute/path/to/Nightly.app --launch-services --frontend-modules bridge/loader-modules/_dist
python3 tools/app-shim/test-runtime.py --browser /absolute/path/to/Nightly.app --launch-services --frontend-modules bridge/loader-modules/_dist --quit-all
python3 tools/app-shim/test-cold-launch.py --browser /absolute/path/to/fully-bundled-Floorp.app
python3 tools/app-shim/test-cold-launch.py --browser /absolute/path/to/fully-bundled-Floorp.app --architecture x86_64
```

Build `bridge/loader-modules` before the browser module tests. Both runners
create isolated profiles. The native runtime runner requires authenticated
handshake, a native window owned by the Shim PID, a real Gecko compositor frame,
a subsequent content repaint, bidirectional cookie sharing, shared local
storage, and real AppKit text input. Native event posting requires an existing
macOS permission; a skipped input test reports a partial result, never a pass.
JSON reports and browser logs are retained under `_dist`.

The popup/overscroll runner keeps native select and anchored-menu preferences
enabled. It moves the PWA window, checks select popup geometry at 100% and 150%
zoom against the Shim's window acknowledgements, requires a presented popup
frame, and checks that an option command reaches the page. It feeds phased
trackpad packets into the Runtime input adapter and checks APZ's overscroll flag,
snap-back completion, and subsequent compositor frames. This exercises the input
and presentation paths; physical trackpad gestures still need manual validation.

`--frontend-modules` also exercises the real installer, native-process recovery
without replacing the live page, beforeunload cancellation, app-only quit, and
both browser-quit settings. The runner uses the Shim bundled in the selected
browser unless `--shim` explicitly overrides it. `--capture-window` captures
only the verified test window; visual verification requires an unlocked desktop
and existing screen-recording permission. These process-local module mounts do
not test Finder cold startup, which needs a separately packaged Floorp frontend.
The cold runner exercises the normal registered command-line handler through
LaunchServices, first with the same-profile browser running and then completely
closed. It checks persistent session sharing, native ownership, fresh frames,
and both browser exit statuses. Its test copy must contain the complete matching
Floorp startup, modules, chrome resources and pages, with a valid local signature.
It accepts both unpacked frontend resources and a packaged `omni.ja`.
It also supplies conflicting restart/reset profile environment variables and
checks that the unrelated profile directories remain untouched. Use
`--inspect-seconds 60` to keep the verified cold window open briefly for manual
inspection before automatic cleanup.

An ad-hoc signed universal test browser currently fails Finder cold launch:
the sealed host requirement contains the running architecture's code hash,
while the native launcher validates that requirement against every architecture.
For local arm64 cold-launch testing, use a separate test copy with an arm64-only
browser executable and re-sign the entire copy. A locally Apple Development
certificate-signed universal browser passed the warm and cold Finder tests on
Apple Silicon. The same tests passed with the x86_64 browser under Rosetta after
the installer made its per-profile Shim x86_64-only before signing it. macOS
otherwise starts a universal Shim as arm64, while the x86_64 Runtime pins the
installed bundle's x86_64 code hash; this mismatch prevents authentication.
The browser package keeps its universal Shim helper so either host architecture
can create its own app. The final package verifier checks both binary slices
and every Runtime patch in the packaged omnijars. These local tests do not
validate the official Developer ID signature, notarized installer, or
execution on an Intel Mac.

Use `--launch-services` for foreground tests: it opens the exact test bundle and
verifies the actual browser PID, isolated profile and process start time before
cleanup. The `open` helper's exit status is recorded separately. Frontend quit
tests establish a real BiDi session because classic WebDriver automatically
accepts beforeunload prompts, which cannot test cancellation.

The native protocol tests validate surface lifetime, transforms, window
ownership, transport verification, bounded text caches, and filesystem
transactions. They do not replace a complete browser run. See the detailed
[native validation matrix](../native/macos-app-shim/README.md) for the current
boundary between tested code and platform integration.

## Remaining platform work

Full remote accessibility/VoiceOver, drag and drop and file promises, native
file/print/permission dialog routing, custom image cursors, and compositor-crash
recovery are not complete. Actual Japanese IME behavior, multiple displays,
sleep/wake, protected media, signed release upgrades, and Finder/profile
remoting with another active profile need scenario testing before enabling this
for general use. Same-profile Finder launches with the browser running and
fully closed have passed against the bundled Floorp frontend, including PWA
chrome initialization, persistent login sharing and normal host shutdown.

Chromium's
[remote_cocoa architecture](https://chromium.googlesource.com/chromium/src/+/refs/tags/135.0.7049.91/components/remote_cocoa/)
and
[App Shim startup](https://github.com/chromium/chromium/blob/main/chrome/app_shim/app_shim_main_delegate.mm)
inform the process boundary. This implementation uses Floorp/Gecko widgets,
compositors, profile ownership, and shutdown policy rather than importing
Chromium's browser runtime.
