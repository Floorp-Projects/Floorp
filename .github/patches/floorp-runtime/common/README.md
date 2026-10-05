# Common Runtime patches

These patches are applied on every platform by `package.yml`, after checking out
Floorp and before the artifact build. Keep Runtime changes here rather than
editing a sibling Floorp-Runtime checkout. Validate patches against the source
commit in `floorp-runtime.lock.json`, using an isolated checkout or source fixture.

`workspace-external-containers.patch` chooses the workspace container before
creating a browser for external URL opens, including cold launch and new windows.
It changes only the existing BrowserContentHandler and BrowserDOMWindow JavaScript
modules; native rebuilding is not required. The matching `tools/patches` patch
maps BrowserContentHandler to the unpacked artifact's `browser/modules` path.
Host policy/patch-parity tests and real HTTP hot/cold isolation runners are described
in `browser-features/chrome/common/workspaces/test/external-container-regression.md`.

`tab-state-and-split-view.patch` preserves Floorp workspace/private-container
session state and N-way split layouts on Firefox 157. The tabbrowser and
sessionstore implementations now live in `moz-src` modules; SessionStore is a
class with private methods. Matching `tools/patches` files cover local builds.
Packaging checks for an exact reverse-applicable patch before applying it, so
JavaScript already included by the source build is not patched twice. Missing
or partially applied patches still fail the build.

`macos-artifact-app-shim.patch` carries the Runtime's native App Shim executable
from the macOS Runtime DMG into the artifact build. The package manifest already
requires that executable; omitting it prevents the final app from being built.

`macos-app-shim-pwa-input-and-compositor.patch` propagates the macOS Widget's
native-compositor policy and keeps the reported backend consistent with it.
PWA windows therefore composite video into their BGRA surface. It also aligns
Shim world coordinates with the sender's int32 domain, preserves IME composition
and selection ranges, and routes phased trackpad input through Gecko's APZ,
overscroll, and history-swipe handling. PWA widgets bypass host-process AppKit
popup menus, which require a local NSView, and use Gecko popups presented by the
Shim instead. Ordinary browser menus keep their existing behavior and native
menu preferences remain enabled. The canonical Shim files in
`native/macos-app-shim/` must stay in sync with this patch. Native View regressions
are covered by `deno task app-shim:build --test`; Runtime phase conversion is
covered by `./mach gtest MacWebAppPanGesture.*`. The isolated
`tools/app-shim/test-popups-and-overscroll.py` runner checks popup positioning,
zoom, option commands, and APZ overscroll against a source-built Runtime.

This patch requires a freshly compiled native Runtime, including XUL and
`floorp-app-shim`. Applying it while packaging older prebuilt native artifacts
does not update those binaries. Existing installed PWAs also need their verified
Shim executable refreshed or reinstalled to receive the coordinate and IME fixes.

`release-notes-guards.patch` adds the confirmed release-notes choice to the existing
extension guard mechanism and includes an xpcshell regression test. It composes
the Floorp guard with existing enterprise guards without replacing their settings.
It uses the Runtime's existing `enterprise-per-extension` source enum value as
guard metadata; no enterprise policy is written. This avoids adding a WebIDL enum
value that would require rebuilding native bindings. The packaging workflow uses
prebuilt native artifacts, so native/WebIDL changes cannot be delivered by this
source-patch step alone.

The default remains unrestricted. A target extension is restricted on
`https://blog.floorp.app` only when `floorp.releaseNotes.choiceConfirmed` is true
and `floorp.releaseNotes.mode` is `support`.

After applying the patch in a Runtime test build, run:

```sh
./mach xpcshell-test toolkit/components/extensions/test/xpcshell/test_ext_floorp_release_notes.js
```

Local development uses the matching `tools/patches/release-notes-guards.patch`
through the existing development patcher. It contains the same JavaScript
changes mapped to the unpacked artifact's `modules/` paths. The host tests check
that the development and packaging patches stay in sync.

The support restriction also covers webRequest/DNR requests and script/CSS
injection in third-party frames whose top document is the HTTPS Floorp blog.
webRequest uses the request ancestor snapshot. Content injection queries the
parent for the top WindowContext before scripts/CSS execute, caching only the
document identity result. Neither uses the currently selected tab.
Changing away from support restores future access; reload open blog pages to
remove already injected content and retry requests.
