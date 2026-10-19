# Chrome sidePanel bridge

`chrome-side-panel.patch` registers a native WebExtension namespace backed by
Gecko's extension sidebar. The development artifact patch contains the same
JavaScript and schema. This requires no new native bindings.

Only converted Chrome extensions with version 1 compatibility metadata and the
matching `<original_id>@chromium-extension` installed ID receive this namespace.
The common child-injection guard is maintained by the offscreen Runtime patch.
Existing Firefox `sidebarAction` APIs keep their schema and permission rules.

Implemented methods are `setOptions`, `getOptions`, `open`, `close`,
`setPanelBehavior`, and `getPanelBehavior`. Panel URLs must belong to the
extension. Opening through the public API requires user interaction. Explicit
window opening selects global options, while selected-tab opening selects that
tab's options. Enabled tabs swap panel content; disabled tabs hide it and retain
the open intent for returning to an enabled tab. Explicit closing clears that
intent. Repeated closing succeeds. Configured Action clicks toggle the panel.

URL tracking is local to each browser window. Switching between explicit tab
configurations and global scope creates a fresh document even when their URLs
match, preventing the previous scope's in-memory state from being reused.
Unconfigured tabs inherit the same global scope.

## Remaining compatibility gaps

This is a partial bridge, not the full Side Panel implementation in the supplied
release plan. Inactive-tab opening/closing currently rejects explicitly. Side
panel events, layout APIs, independently retained documents for every tab, and
Floorp panel pinning/shortcut management are not implemented. Switching explicit
scopes recreates the document instead of retaining each scope's live state.
Converted extensions using those features still need further work; no successful
placeholder results are returned for unsupported methods.

## Verification

The source patch includes `browser_ext_floorp_sidePanel.js` for browser-chrome
runs against the locked Runtime revision. Its prerequisite compatibility schema
comes from `chrome-extension-native-messaging.patch`.

```sh
deno test --allow-read tools/chrome-side-panel.test.mts
python3 tools/chrome-extension-runtime/test-side-panel.py --browser _dist/bin/floorp
```

The second command uses a private profile and a temporary installed extension. It
checks child API bindings, real sidebar documents, Action clicks, tab switching,
disabling/restoration, scope isolation for equal URLs, and repeated closing.
It writes its report under `_dist/side-panel-*/report.json`. Linux execution does
not establish Windows/macOS package compatibility or the plan's real-extension
acceptance target.
