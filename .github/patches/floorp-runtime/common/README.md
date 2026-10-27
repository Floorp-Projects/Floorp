# Common Runtime patches

These patches are applied on every platform by `package.yml`, after checking out
Floorp and before the artifact build. Keep Runtime changes here rather than
editing a sibling Floorp-Runtime checkout. Validate patches against the source
commit in `floorp-runtime.lock.json`, using an isolated checkout or source fixture.

Packaging skips an exact reverse-applicable common patch that was already applied
by the Runtime's upstream patch step. A missing patch is applied normally, while
partially applied patches still fail validation. Native changes still require
freshly compiled Runtime artifacts.

`workspace-external-containers.patch` chooses the workspace container before
creating a browser for external URL opens, including cold launch and new windows.
It changes only the existing BrowserContentHandler and BrowserDOMWindow JavaScript
modules; native rebuilding is not required. The matching `tools/patches` patch
maps BrowserContentHandler to the unpacked artifact's
`moz-src/browser/components` path on Firefox 159; BrowserDOMWindow remains in
`browser/modules`. Both paths import the helper through its native
`moz-src:///browser/components/BrowserContentHandler.sys.mjs` module URI.
Host policy/patch-parity tests and real HTTP hot/cold isolation runners are described
in `browser-features/chrome/common/workspaces/test/external-container-regression.md`.

`tab-state-and-split-view.patch` preserves Floorp workspace/private-container
session state and N-way split layouts on Firefox 159. The tabbrowser and
sessionstore implementations now live in `moz-src` modules; SessionStore is a
class with private methods. Matching `tools/patches` files cover local builds.
Packaging checks for an exact reverse-applicable patch before applying it, so
JavaScript already included by the source build is not patched twice. Missing
or partially applied patches still fail the build.

`contextual-search-initialization.patch` lazily loads the selector configuration
for contextual engine lookups. Floorp's fixed default engine list can initialize
SearchService without loading that configuration. If it is unavailable, the
optional lookup returns no match so OpenSearch and other address-bar actions
can continue. The matching `tools/patches` patch covers extracted local builds;
the host parity test and browser `contextualSearch.test.ts` cover both paths.
This changes JavaScript only and does not require native recompilation.

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
menu preferences remain enabled. PWA context-menu input uses Gecko's pointer
event class so that remote content processes accept the event without an IPC
deserialization failure. The canonical Shim files in
`native/macos-app-shim/` must stay in sync with this patch. Native View regressions
are covered by `deno task app-shim:build --test`; Runtime phase conversion is
covered by `./mach gtest MacWebAppPanGesture.*`. The isolated
`tools/app-shim/test-popups-and-overscroll.py` runner checks popup positioning,
zoom, option commands, and APZ overscroll against a source-built Runtime.
`tools/app-shim/test-context-menu.py` checks repeated trusted right-click menu
delivery and content-process survival through the PWA input adapter.
The Shim retains the IME's selected conversion clause while Gecko acknowledges
a collapsed caret; repeated Space presses continue Japanese conversion. The
selection override ends on commit, unmark, cancellation, empty marked text,
leaving an editable element, or disconnection. Candidate geometry uses that same
selection. `tools/app-shim/test-ime.py` covers real native Japanese input in both
textarea and contenteditable fixtures, using an isolated browser profile.

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

`chrome-extension-native-messaging.patch` adds an extension-scoped Chrome native
messaging bridge. It recognizes
`browser_specific_settings.gecko.floorp_chrome_extension` with
`schema_version: 1` and a 32-character lowercase `a`–`p` `original_id`, and requires
the installed Gecko ID to remain `${original_id}@chromium-extension`. This keeps
permissions, storage, and the Firefox native-host protocol tied to the existing
addon identity. With `chrome-extension-messaging.patch`, compatible contexts see
the original Chrome `runtime.id`; Gecko's internal routing ID remains unchanged.
The marker selects compatibility behavior and does not authenticate installed-XPI
provenance. The converter separately verifies CRX signatures and the developer
public key's match to the requested Chrome ID before conversion; installed
manifest/report metadata alone does not prove that verification occurred or that
the installed package came from a store.

Initial installs and manual updates share a credential-omitting streamed
downloader with a 100 MiB limit and five-minute timeout per configured endpoint.
Both declared and actual byte counts are checked. Authenticated ZIP conversion
also limits expanded entries to 50 MiB each, 500 MiB total, and 10,000 entries.
Download bounds and signature verification have separate regression coverage.

Firefox native hosts keep priority. Opted-in callers may additionally discover
Chrome and Chromium hosts in their standard user/system directories or Windows
32/64-bit registry views. A denied or malformed installed host blocks fallback
to another executable. Chrome hosts must explicitly authorize the exact
`chrome-extension://<original_id>/` origin in `allowed_origins`; wildcard origins
are refused. After authorization, Gecko's existing native-manifest validation
checks the host name, type, and executable path. Chrome hosts receive their
Chrome origin as the first process argument, plus `--parent-window=0` on Windows.
Firefox hosts retain `[manifestPath, GeckoAddonID]` arguments.

These JavaScript and schema changes do not require native recompilation. The
existing sandbox portal/proxy path continues to use Mozilla's native-messaging
protocol and does not gain Chrome host discovery. Windows and macOS execution
remain unverified. The matching development patch is
`tools/patches/chrome-extension-native-messaging.patch`.

The source patch adds xpcshell regressions to the existing native-manifest suite:

```sh
./mach xpcshell-test toolkit/components/extensions/test/xpcshell/test_native_manifests.js
```

The Linux browser integration runner reconstructs the source fixture from the
locked Runtime commit and applies this patch in isolation. It launches an actual
Python native host, verifies Chrome and Firefox process arguments and framed
JSON round trips, and checks metadata, authorization, and host precedence. It
leaves the Runtime checkout and browser artifact unchanged:

```sh
python3 tools/chrome-extension-runtime/test-native-messaging.py
```

`chrome-extension-offscreen.patch` registers an extension-owned hidden document
host and native `OFFSCREEN_DOCUMENT` contexts. Converted extensions receive only
runtime APIs inside that document and its extension frames. Creation, audio
silence, background suspension, document replacement, and extension shutdown have
dedicated regression coverage. The former background iframe fallback is retired.

`chrome-side-panel.patch` connects the Chrome `sidePanel` API to Gecko's sidebar
rendering and action buttons. Its implemented methods and remaining scope/lifetime
differences are documented in the [compatibility runtime guide](../../../../docs/development/features/browser-features/modules/chrome-extension-runtime.mdx).
Both APIs require the explicit compatibility metadata and matching installed ID.
Matching `tools/patches` files deliver these JavaScript changes to local artifacts.

Validate all seven Chrome Runtime patches together against the locked source and
check that the development patches contain the same changes:

```sh
python3 tools/chrome-extension-runtime/check-patches.py
```

The seven source/development pairs are native messaging, Offscreen, Side Panel,
messaging, proxy, diagnostics, and capture. They compose against the pinned
Firefox 157 Runtime; none adds compiled Worker bindings. See the
[native extension Worker assessment](../../../../docs/development/features/browser-features/modules/chrome-extension-service-workers.mdx)
for the required native rebuild and remaining API/registration work.

`chrome-extension-messaging.patch` exposes the original Chrome `runtime.id` to
explicitly converted extensions while retaining the Gecko addon ID for internal
routing, permissions, storage, and native `moz-extension://` URLs. The shared
`ExtensionUtils.getChromeExtensionId(extension)` helper validates compatibility
metadata against the installed ID. `runtime.getURL()` keeps its working native
URL resolver.

Compatible callers can address `runtime.sendMessage()` and `runtime.connect()`
with another original Chrome ID only when the exact corresponding opted-in
extension is installed. Parent-computed internal identities select same-extension
versus external events before the public sender ID is translated. Compatible
receivers see compatible senders' original IDs; Firefox receivers continue to
see Gecko IDs.

For compatible targets, `externally_connectable.ids` is enforced before waking
the target. Exact original IDs, explicitly translated Gecko IDs, and `"*"` are
supported. A generated Gecko-ID suffix alone does not satisfy an original-ID
allowlist. When the field is present without `ids`, extension callers are denied;
when absent, extension callers retain the Chrome default of being allowed.
Firefox targets retain their existing external-messaging behavior. Web-page API
injection and `externally_connectable.matches` authorization remain unimplemented;
this patch does not expose extension APIs to matching web pages.

The matching development patch is `tools/patches/chrome-extension-messaging.patch`.
The installed-extension browser test checks actual messages and Ports, allowed
and denied callers, original public IDs, same-extension event classification,
Firefox behavior, suffix-only impersonation refusal, and native URL loading:

```sh
python3 tools/chrome-extension-runtime/run-browser-tests.py \
  --browser _dist/bin/floorp/floorp \
  --test browser-features/modules/modules/chrome-web-store/test/ChromeMessagingNative.test.mts
```

`chrome-extension-proxy.patch` translates compatible callers' Chrome
`proxy.settings` values for `direct`, `auto_detect`, `system`, `fixed_servers`,
and `pac_script`. Fixed-server rules are compiled into PAC; inline and URL PAC
run through Gecko's real network backend. The bridge preserves native setting
ownership, clear/restore behavior, callbacks/Promises, and persistent `onChange`
events. HTTP/HTTPS/SOCKS4/SOCKS5 endpoints and supported hostname bypass patterns
are accepted. Unsupported modes, QUIC, mandatory PAC, unsupported bypass patterns
such as CIDR, and incognito-specific settings reject before mutation. Only the
regular scope is supported. Modifying settings retains Gecko's requirement that
the extension be allowed in private windows. Firefox callers keep their native
settings format. The private-profile runner checks actual fixed-server and PAC
traffic, bypass behavior, control events, and Firefox regression behavior:

```sh
python3 tools/chrome-extension-runtime/test-proxy.py --browser _dist/bin/floorp/floorp
```

`chrome-extension-diagnostics.patch` adds a section to existing expanded
`about:addons` details for current and recognized legacy converted extensions.
It reads recorded identity/version/schema, conversion warnings, and native live
contexts; provides active-extension reload, refresh, and JSON export; and includes
English/Japanese labels. Legacy metadata is labeled unverified and does not grant
compatibility APIs. Recognized signature fields in an export remain recorded
converter metadata, not independent installed-package provenance. Actual native
UI interaction coverage passes 28 checks, including export/reload and translations:

```sh
python3 tools/chrome-extension-runtime/test-diagnostics-ui.py --browser _dist/bin/floorp/floorp
```

The same section exposes explicit Check, Reapply, and Apply controls through the
Floorp `ChromeExtensionUpdater` module. Checks authenticate and convert a CRX
candidate without installing it; a separate Apply uses the normal Add-on Manager
permission flow. Explicit same-version reconversion supports migration while
preserving the internal ID, data, and granted permissions. Native persistent
installation tests cover updates/reconversion, permission rejection, competing
installs, cancellation, and postponed installation. Candidate expiry and changed
installed versions require a fresh check; the exact existing version and prepared
candidate are checked before the native permission flow. Automatic updates and
migration are not implemented. The UI fixture substitutes only its service within its private
browser process; separate native updater tests exercise the production service.

`chrome-extension-capture.patch` implements video-only `tabCapture.capture`,
`getMediaStreamId`, `getCapturedTabs`, and `onStatusChanged`. It requires actual action invocation for the
selected target document; hover/preload and generic `activeTab` grants do not
authorize capture. Opaque tokens expire after ten seconds and can be consumed
once by the same extension, including its Offscreen document. Tab audio, desktop
capture tokens, and `consumerTabId` reject explicitly. This patch does not remove
ordinary browser screen-capture user-interaction checks. Native tests verify
real video pixels and WebM recording, background suspension independence, and
track termination on target navigation, source-tab closure, and consumer shutdown.
A separate same-principal fake-camera test observes the actual native permission
prompt and explicit denial while a capture backend permit is pending.

The native Side Panel runner passes 17 checks. Switching configured scopes
recreates their documents, including equal URLs; independently retained per-tab
documents remain unimplemented. These Linux fixture results do not establish a
compatibility percentage or qualify Windows/macOS/Linux release packages. The
[compatibility runtime guide](../../../../docs/development/features/browser-features/modules/chrome-extension-runtime.mdx)
records the complete implemented scope, commands, and remaining release work.

The reference comparison now uses official Google Chrome `155.0.8059.84` for
Linux amd64, verified through signed repository metadata. Its package SHA-256 is
`8fbd2ae69cdf7ee98b7513c3396a7b5e06f974f8625fdd590df3091dd0c3e9de`; the executable
SHA-256 is `cb19b59995072b86cc7f887b3b1c7ab1f63518a7f2ed9f0d33249e1a040e2c76`.
The completed self-authored fixture comparison covers public identity, messages
and Ports, native URL resource loading, contexts/filters, and Offscreen lifecycle.
It reports the actual Service Worker versus DOM event-page and native URL scheme
differences explicitly. This is one fixture; the planned 50-extension corpus and
its exact versions/operations remain unfrozen. Run it with:

```sh
node tools/chrome-extension-runtime/compare-reference.mjs \
  --chrome _dist/chrome-reference/extracted/opt/google/chrome/chrome \
  --floorp _dist/bin/floorp/floorp
```

### Chrome debugger protocol adapter

`chrome-extension-debugger.patch` exposes `debugger` only to extensions with the
validated Chrome compatibility marker and `debugger` permission. The parent owns
exclusive sessions and uses a local Gecko DevTools client and native target,
inspector, console, resource, configuration, and screenshot actors. It never
starts a remote debugging listener. Native target lifetimes invalidate remote
object and DOM handles across process switches; explicit detach, tab close,
permission removal, developer-tool takeover, and extension shutdown release
actors and restore owned configuration. Native document principals supplement
URL checks, preserving other add-ons' ownership of inherited `about:blank`
documents. Persistent events wake a stopped background. Native target-process input and file input actors produce actual
widget, editor, and file selection effects.

`chrome-extension-debugger-native.patch` is compiled-only. Parent-only
BrowsingContext fields identify the active owning extension and enable genuine
focus/visibility emulation and CSP bypass. Native checks still reject restricted,
quarantined, guarded, and unauthorized file targets. These two protocol commands
reject clearly on a binary without this patch. Session teardown restores previous
values; inactive or permission-revoked owners cannot enable the overrides.

The adapter implements a named command subset of Runtime, Page, DOM, Network,
Input, Target, and Emulation. Unsupported commands and parameters reject; it does
not advertise complete CDP domains. Exact signed-package literals, source hashes,
and observed parameters are frozen in
`tools/chrome-extension-runtime/debugger-command-inventory.json`. Literal scanning
and authored protocol checks are separate from running each Store extension's
primary operation.

Run `tools/chrome-extension-runtime/run-browser-tests.py --browser <binary>
--test browser-features/modules/modules/chrome-web-store/test/ChromeDebuggerNative.test.mts`
for native ownership, Firefox isolation, object/DOM lifetimes, modified keyboard
and mouse effects, file upload, prompts, network bodies, process switching, and
background wake. Run `node tools/chrome-extension-runtime/compare-debugger.mjs`
for the same public API contract against the hash-verified official Chrome 155
reference. Add `--native-overrides` with a compiled candidate to compare focus,
visibility, CSP enforcement/bypass, and restoration after detach. These fixtures
are self-authored and make no real-extension pass-rate claim. The separate
`ChromeDebuggerPrincipalNative.test.mts` uses two actual authored add-ons and
trusted native input to verify public Page/Runtime attachment preserves an
inherited document's extension principal.

### Storage, action, notifications, and declarative content

`chrome-extension-auxiliary-content.patch` scopes Chrome additions to the exact
validated compatibility marker. `storage.*.setAccessLevel` changes real parent
storage authorization, child IndexedDB access, and change-event delivery. Session
storage starts trusted-only; grant and revoke affect existing content-script
handles. Firefox extensions retain their native API surface. Compatible
`action.openPopup` uses the existing browser action UI without a global gesture
preference change.

Compatible notifications expose actual permission/readback/update operations and
forward action buttons, silent delivery, and interaction requirements to native
Gecko alerts. Notification identities include the owning extension ID; replacing
an alert preserves its public ID without generating a stale close event.
`chrome-extension-notifications-native.patch` additionally forwards existing
native `nsIAlertAction` objects into the browser alert fallback window. That part
requires a compiled binary. The browser window renders native controls and routes
button index events through the existing persistent event system. Existing OS
alert implementations use their native action support.

`ChromeNotificationsCloseNative.test.mts` waits for an actual browser alert,
clears it through the extension API, and verifies exactly one native
`onClosed(id, false)` event. It also replaces an alert and verifies the old
instance cannot remove the current notification. Compiled native action buttons
remain covered separately by `ChromeAuxiliaryNative.test.mts`.

`declarativeContent` provides constructible condition/action records and real
per-extension persisted rule storage. Native document actors evaluate URL, CSS,
and bookmark conditions. Matching rules affect the existing native action UI;
separate overlays preserve imperative state when rules stop matching. Authorized
`RequestContentScript` actions execute extension files through Gecko's normal
content-script injection checks. Rules continue after background suspension,
reload from disk on extension startup, and are removed on uninstall. Permission
changes invalidate pending matching work before effects or injection. URL regex
conditions share the converted-extension regex normalizer and native bounded
engine used by DNR; that implementation's documented RE2 limits also apply here.

`tools/chrome-extension-runtime/auxiliary-api-inventory.json` records exact signed
package versions, package/source hashes, and relevant member references. Source
references include feature detection and do not establish a primary-operation
pass. `compare-auxiliary.mjs` runs an authored public API contract against the
frozen official Chrome reference and the supplied Floorp binary. The native
lifecycle test is `ChromeDeclarativeContentNative.test.mts`; the strict compiled
alert-window test is `ChromeAuxiliaryNative.test.mts`. These checks do not claim
complete notification rich-template or declarative-content selector parity.
