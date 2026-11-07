<!-- SPDX-License-Identifier: MPL-2.0 -->

# Tab Stacks startup and session regression coverage

Run the colocated tests inside the locked native Runtime:

```sh
deno task test --near browser-features/chrome/common/tab-stacks --layer chrome
```

The startup tests control window readiness independently and exercise the real
Solid reactive roots. They cover synchronous startup, deferred startup, repeated
initialization, preference changes while waiting, fresh chrome references,
startup rejection, partial initialization failure, and cancellation on unload or
HMR. Simulated recovery ordering is distinct from native SessionStore testing.

The SessionStore tests open real browser windows through the normal feature
loader, create native groups, and restore closed windows. They verify the second
tab row, explicit native-group presentation, and independent window lifetimes.
The existing native/interaction suites cover drag, selection, split-view and
orientation behavior.

For full-process normal startup, graceful session restoration, and forced-kill
recovery on Windows, run `deno task test:tab-stacks-session` with a built
overlay and its development servers running; use `--help` for the binary and
output options. It uses an isolated profile and only terminates its own browser
process. These process-level checks are separate from the colocated test runner
because restarting its browser would also terminate that runner.

## Scope of issue #2890

The original Windows 11 / Floorp 12.20.1 report retains tabs and groups but
loses the Tab Stacks presentation. The loader waits for process-wide
`SessionStore.promiseInitialized`; this does not guarantee each restored window
has a usable `gBrowser`, tab container, head or toolbar insertion point.

The reporter's delayed-startup retry addresses that race, but its detached
`createRoot(() => this.init())` discards the disposer. The implementation
instead links the initialization root explicitly to the existing component
lifetime, cancels late continuations, guards repeated calls, and rolls back
partial mounts. The window startup observer is a fallback for runtimes without
the promise; it filters the owning window and is removed on success or disposal.

The reactive stack row renders inside a captured, feature-owned mount with
`display: contents`. Removing that mount on cleanup also removes the current
dynamic row; the shared renderer's `insert()` return value does not retain DOM
when its input is a function accessor. The native toolbox and navigation bar
remain outside that mount.

The October 15 NixOS / Floorp 155 comment describes windows with no tabs or
title. That is a separate symptom: Tab Stacks only changes presentation of
native groups, and this fix does not establish the cause of missing native
session data. Investigate that report with the native SessionStore state,
runtime/version, startup errors, and extensions recorded separately. Do not
infer that a passing overlay test verifies the published Floorp 12.20.2 package.
