# Last-tab preference regression (#2707)

`workspaceLastTab*.test.ts` and `nativeLastTab*.test.ts` exercise real browser
windows, their workspace managers, native tab removal, and SessionStore. Run
them against a Runtime containing
`.github/patches/floorp-runtime/common/workspace-last-tab.patch` (packaging) or
`tools/patches/workspace-last-tab.patch` (local development):

```sh
deno task test --near browser-features/chrome/common/workspaces/test --layer chrome
```

Use a disposable profile. The tests restore workspace data and preferences and
close only windows they create. Their closed-window entries can remain in that
test profile's session history.

Expected behavior:

| Native close-with-last-tab preference | Hidden workspace tabs in this window | Workspace exit setting | Result                                                                       |
| ------------------------------------- | ------------------------------------ | ---------------------- | ---------------------------------------------------------------------------- |
| false                                 | no                                   | either                 | Keep the window with one replacement tab                                     |
| true                                  | no                                   | either                 | Native window closure, including when another window has hidden tabs (#2684) |
| false                                 | yes                                  | either                 | Keep the window and switch to the remaining workspace                        |
| true                                  | yes                                  | false                  | Keep the window and switch to the remaining workspace                        |
| true                                  | yes                                  | true                   | Preserve a replacement and hidden workspace tabs in the closing session      |

The native preference is never written by Workspace. The Runtime prevents its
last-visible-tab fast path from discarding hidden tabs in a different Floorp
workspace in the same window, only while Workspaces is enabled. Both the closing
and hidden tab must have different, nonempty workspace IDs. Extension-hidden
tabs in the current workspace and tabs without workspace attribution retain
native behavior. Disabling Workspaces also bypasses this guard when restored
tabs still carry stale workspace attributes. Explicitly closing all tabs,
including the hidden ones, retains the native bulk-close behavior.

The suite covers both native preference values with Workspaces on/off, native
`hideTab` (including an extension source), missing workspace attribution,
disabled-feature stale attribution, and collapsed Firefox groups. On the locked
Firefox 156 Runtime, a collapsed group's tab is not `hidden`; it is still an
open tab for the native last-tab calculation, so closing the ungrouped tab keeps
the window open with either preference value.

The 22 cases share `workspace-last-tab-test-utils.ts` and are split across five
test modules. Each module creates at most six windows, keeping slow development
startup within the runner's existing 120-second per-module timeout.

Run in test mode to cover the startup runner as well: windows created by the
suite must initialize their features without starting a second suite or
overwriting the primary window's results. The startup test-run ownership guard
is process-local; restarting the browser must allow a new run.

For restart coverage, set `browser.tabs.closeWindowWithLastTab=false` through
`Services.prefs` or about:config (not user.js), enable session restore, and keep
a nonblank tab in a hidden workspace. Quit the disposable browser normally,
launch the same profile, and verify the preference remains false and the hidden
tab's URL and workspace attribution survive. Then close the last visible tab and
verify the window remains open. Repeat with true and verify an ordinary last-tab
window closes. Do not infer the original preference value for profiles already
changed by older Floorp versions: the previous writer did not record the user's
intent.
