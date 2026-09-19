# Last-tab preference regression (#2707)

`workspaceLastTabPolicy.test.ts` exercises real browser windows, their workspace
managers, native tab removal, and SessionStore. Run it against a Runtime
containing `.github/patches/floorp-runtime/common/workspace-last-tab.patch`
(packaging) or `tools/patches/workspace-last-tab.patch` (local development):

```sh
deno task test --near browser-features/chrome/common/workspaces/test/workspaceLastTabPolicy.test.ts --layer chrome
```

Use a disposable profile. The tests restore workspace data and preferences and
close only windows they create. Their closed-window entries can remain in that
test profile's session history.

Expected behavior:

| Native close-with-last-tab preference | Hidden tabs in this window | Workspace exit setting | Result                                                                       |
| ------------------------------------- | -------------------------- | ---------------------- | ---------------------------------------------------------------------------- |
| false                                 | no                         | either                 | Keep the window with one replacement tab                                     |
| true                                  | no                         | either                 | Native window closure, including when another window has hidden tabs (#2684) |
| false                                 | yes                        | either                 | Keep the window and switch to the remaining workspace                        |
| true                                  | yes                        | false                  | Keep the window and switch to the remaining workspace                        |
| true                                  | yes                        | true                   | Preserve a replacement and hidden workspace tabs in the closing session      |

The native preference is never written by Workspace. The Runtime prevents its
last-visible-tab fast path from discarding hidden tabs in the same window; it
does not change the profile-wide preference. Explicitly closing all tabs,
including the hidden ones, retains the native bulk-close behavior.

For restart coverage, set `browser.tabs.closeWindowWithLastTab=false` through
`Services.prefs` or about:config (not user.js), enable session restore, and keep
a nonblank tab in a hidden workspace. Quit the disposable browser normally,
launch the same profile, and verify the preference remains false and the hidden
tab's URL and workspace attribution survive. Then close the last visible tab and
verify the window remains open. Repeat with true and verify an ordinary last-tab
window closes. Do not infer the original preference value for profiles already
changed by older Floorp versions: the previous writer did not record the user's
intent.
