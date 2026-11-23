# Workspace and tab Sync v1

Status: implementation contract, authenticated two-client cloud acceptance and
physical Mac-to-Windows round-trip acceptance passed.

Floorp Desktop uses Firefox Sync's authenticated, encrypted transport for an
independent `floorp-workspaces` collection. The engine version is `1`; its
switch is `services.sync.engine.floorp-workspaces`. It is enabled by default,
but does not run while `floorp.workspaces.enabled` is false. Signing into a
Mozilla Account and enabling Firefox Sync are prerequisites for transport.

A workspace's context menu has **Sync This Workspace and Its Tabs**. This is a
local subscription choice, separate from the currently displayed workspace.
Existing workspaces are not uploaded until selected. Incoming workspace metadata
subscribes that workspace on Desktop and opens its web tabs in the background.
Other receivers, including a future iOS implementation, may offer discovery or
an explicit subscribe step instead; automatic opening is not a wire requirement.

Unchecking keeps local content and the account's copy, and suppresses incoming
changes for that workspace. Re-enabling fetches the collection again to include
changes made while unsubscribed. Deleting or archiving a subscribed workspace
uploads tombstones for its metadata and tabs. Other Desktop installations stop
synchronizing that workspace and retain their local content. A deleted Sync UUID
cannot be reused: archive restoration and new workspaces generate new UUIDs.

## Records

There is one BSO for workspace metadata and one BSO for each live tab. Workspace
and tab IDs are distinct, random UUID strings, globally unique in the
collection. Preserve received IDs exactly; new implementations should generate
lowercase UUIDs. Moving a tab to another workspace or explicitly reopening a
closed tab uses a new tab ID. Session restoration alone must not revive a
retired ID.

The following examples are the **decrypted CryptoWrapper cleartext**, including
its outer `entity` field. They are not the encrypted HTTP BSO envelope.

```json
{
  "id": "11111111-1111-4111-8111-111111111111",
  "entity": {
    "version": 1,
    "kind": "workspace",
    "workspaceId": "11111111-1111-4111-8111-111111111111",
    "name": "Research",
    "icon": "book",
    "metadata": {
      "modifiedAt": 1792280000000,
      "modifiedBy": "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"
    }
  }
}
```

```json
{
  "id": "22222222-2222-4222-8222-222222222222",
  "entity": {
    "version": 1,
    "kind": "tab",
    "workspaceId": "11111111-1111-4111-8111-111111111111",
    "tab": {
      "id": "22222222-2222-4222-8222-222222222222",
      "url": "https://example.com/",
      "title": "Example",
      "pinned": false,
      "position": 0,
      "deleted": false,
      "modifiedAt": 1792280000000,
      "modifiedBy": "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa"
    }
  }
}
```

A deletion is a standard Sync tombstone for the same BSO ID. Its decrypted
cleartext is `{ "id": "…", "deleted": true }`, with no `entity` required. There
is no custom REST client, token storage, encryption scheme, or account system.
Application Services clients must use the normal collection keys and Sync
`meta/global` engine contract, not upload the inner `entity` unwrapped.

`modifiedAt` is a non-negative safe integer in Unix milliseconds. A local edit
advances beyond both the wall clock and every clock already observed in that
workspace. `modifiedBy` is a persisted random device UUID, not an account token
or FxA device identifier. Metadata and each tab merge independently by
`(modifiedAt, modifiedBy)`. Equal clock/device conflicts use the lexically
larger JSON value with the canonical field order produced by the TypeScript
parser; receivers should reproduce that order and standard JSON escaping. UUID
ordering is case-sensitive. Duplicate-ID records and workspace ownership changes
for an existing tab ID are rejected.

Deletion wins over every live version of the same UUID, including an offline
client's newer navigation or changed position. Closed-tab payloads are removed
from the profile cache only after the corresponding standard tombstone is
successfully committed. A lightweight, account-bound retired UUID set remains
locally, preventing stale records or restored sessions from reviving those IDs.
The set contains IDs only, and does not consume wire payload space. There is no
cumulative tab-count limit or monolithic workspace payload.

Tabs may arrive before metadata; they are staged without opening a placeholder
workspace. The first actual metadata record supplies its name and icon even when
its valid clock is zero. Subsequent metadata records use the normal merge. A
parent tombstone suppresses its later metadata and child records regardless of
arrival order, including on a client that never saw that workspace before.

## Tab content and order

Only HTTP(S) URLs without embedded credentials are synchronized. Private
windows, local files, privileged/internal pages, extension URLs, forms, cookies,
history, SessionStore state and local container numbers are excluded. Names and
icons are at most 256 UTF-16 code units, titles 1024, and URLs 8192. Strings
must be valid Unicode. Desktop truncates long titles without leaving unpaired
surrogates.

When an already-synced tab navigates to an unsupported local page, its last
canonical web URL, title, pin state and ID remain cached. The local page's URL
and title are never captured, and an incoming update does not navigate that
local page or change its pin state. Returning to HTTP(S) resumes capture with
the same ID. Moving the physical tab between workspaces creates a new ID even
when capture of the source workspace has already retired its previous ID.

Each canonical inner entity has a 16 KiB UTF-8 ceiling, leaving ample room for
CryptoWrapper's ID, encryption padding, base64 expansion and BSO metadata under
Firefox's normal 256 KiB record limit. The wire helpers validate outgoing and
incoming entities before they are saved or used to navigate.

Container assignment remains local. A received tab uses that workspace's
existing local default container; a newly discovered workspace uses container 0.
The sender's numeric container ID is never interpreted on another installation.

A tab's position is synchronized, but window placement, active workspace and
active tab are local. Receiving a new window distribution does not itself change
positions: each provider baselines its physical order on first capture and after
applying remote data, and only an actual subsequent local order change restamps
existing positions. An initial capture preserves existing positions and appends
new tabs. When a user changes the order across local windows, their combined
local order is published; the receiver preserves its own window distribution.

## Lifecycle and failure behavior

All normal browser windows must finish SessionStore restoration and register
before capture can interpret an absent tab as closed. Registration is cancelled
on window disposal, has a 30-second bound, and retries with backoff on transient
initialization failures. A missing provider can fail this engine's attempt; it
cannot leave the entire Firefox Sync operation waiting indefinitely. Private
windows do not participate in the barrier.

With no normal-window provider, incoming UI changes remain pending until a
provider registers. Each native incoming batch applies the accumulated UI state
once per changed workspace; provider failures stay associated with the affected
record IDs for retry. Global workspace deletion also creates its tombstone when
initiated in a private window, without registering that window as a tab
provider.

Incoming tab closures are persisted and deferred until all incoming batches and
backfill have succeeded. A parent deletion then cancels its children's deferred
UI closures, retaining the local workspace. Unresolved decrypt/reconcile/apply
failures are independently persisted and retried even after Firefox's usual
repeated-failure list would drop them. Pending closures remain deferred across
restart; capture excludes their retired IDs. Unsubscribing retains deferred tab
closures until successful application after reselection, so a closed tab cannot
be republished during that transition.

Before merging incoming records or flushing tab closures, the engine captures
local edits that occurred while network requests were in flight. An unrelated
incoming tab or metadata record therefore does not rewind a pending local
navigation. A persisted pending-apply marker and pre-apply local baseline
protect remote tabs from being mistaken for locally closed tabs after a failed
or partially completed UI apply. Recovery captures only changes since that
baseline, including navigation, explicit closure and reordering of previously
observed tabs, while retaining remote additions that never opened. Registration
and the next capture retry the desired state. Pending remote closures are
retried only after a completed incoming pass, so a later parent deletion can
still cancel them.

If a failed server record is physically removed rather than repaired, this
conservative policy can keep deferred closures pending. Turning off workspace
sync preserves local content; disconnecting Firefox Sync clears the account's
workspace cache and subscriptions, after which the user can reconnect and opt in
again. This reset is intentional and does not delete local browser content.

Disabling workspaces also aborts in-flight provider work and rejects incoming
and outgoing reads from an already-started native Sync operation. Rejected
incoming IDs retain the native and persisted failure markers. Re-enabling resets
the download cursor so records are fetched again even if the earlier native
download advanced it; pending UI state is retried while deferred closures still
require a completed incoming pass.

An incoming URL change retains its URL and title intent until a top-level native
navigation commits, so a capture cannot publish the previous page as a newer
local edit. Each provider apply waits at most 30 seconds; that wait's timeout or
another tab's failure leaves a still-owned load and its capture intent intact. A
retry reuses the live navigation only for the same URL and cancellation signal.
A newer remote URL supersedes the prior owned load, including when it equals the
still-visible old page. Real navigation failures and no-document responses
release only their own intent. Account or feature abort and window disposal stop
owned loads. Genuine local navigation and Back supersede the intent without
masking subsequent local capture. The wait does not classify user-created tabs
as incoming tabs, and late completion schedules capture and pending-apply
recovery.

`weave:service:start-over` synchronously invalidates the account session and
aborts pending provider work before queuing the cached-state reset. Incoming
records retain their originating session; store mutations, outgoing reads and
asynchronous continuations reject invalidated sessions. Providers recheck the
cancellation signal before changing UI after an asynchronous wait. Every engine
startup also checks the signed-in FxA UID against the stored owner before
uploading, and rejects a UID lookup that crossed account invalidation. A
different account starts with local-only workspaces and no inherited remote
subscriptions or retired IDs.

Invalid local metadata pauses only that workspace and retains its last good
record. Its menu shows an attention state and a localized explanation; failed
initial selection rolls back and displays an alert. Other workspaces continue
syncing. Correcting the metadata resumes capture and requests a fresh download.
Invalid captured baselines are never persisted. An invalid baseline from an
older cache is isolated without rejecting the entire cache or discarding its
pending-apply marker.

## Build paths and validation

Production registration is in
`.github/patches/floorp-runtime/common/workspace-sync.patch`. `package.yml`
applies all common source patches before packaging even when native Runtime
artifacts are reused; this JavaScript-only patch needs no Gecko native
recompilation. Development patches extracted Runtime code using
`tools/patches/modules-services-sync-service.sys.patch`. The host compatibility
test asserts that these changes are identical after path mapping.

The pure model and wire tests are browser tests under
`browser-features/modules/test/workspaces/`. They cover concurrent additions,
offline close plus reorder, clock regression/ties, payload and Unicode
boundaries, per-tab entities, and large aggregates. Implementation validation
also exercised the native Sync engine/record APIs and encryption in an isolated
Gecko 158 profile. The production Chrome and ESM loader builds also succeed. A
reproducible fresh-profile harness is documented in
`tools/workspace-sync/README.md`.

Review follow-up at revision `78aedc2cc830` passed 58 fresh-profile cases: 23
model/wire, 24 native engine and 11 browser-tab/SessionStore cases. These
include provider absence, batch UI application and failure recovery,
unsubscribed tab closures, invalid persisted baselines, private-window global
deletion, source-first tab moves, and unsupported-page transitions. Nine
isolated negative controls failed their intended assertions when the
corresponding fixes were removed. The full smoke task and production Chrome/ESM
builds also passed at this revision.

Authenticated cloud acceptance on 2026-10-18 passed eight scenarios with two
independent QA app processes/profiles on one Mac and a dedicated Mozilla
Account. The copied complete Floorp apps used production Chrome/ESM outputs and
the native registration patch. Tests exercised actual HTTPS Sync transport,
bidirectional metadata/tab changes, concurrent additions, close versus stale
reorder, empty workspaces, local opt-out/re-selection and parent deletion.
Successful cleanup removed only generated synced records; other engine metadata
and declined settings had the same SHA-256 fingerprint before the first explicit
acceptance Sync and after cleanup. The initial interactive account setup
precedes that fingerprint boundary. The launcher, configuration preflight and
repeatable driver are documented in the same README.

All eight cases passed again after the review fixes at `78aedc2cc830`, with the
normal production Chrome/ESM build arguments, byte-verified app resources and
the existing dedicated account profiles. The Windows QA client was stopped
during this run. Cleanup again removed only generated records and preserved
other engine metadata/declined settings. The README records this newer engine
hash separately from the earlier physical and normal-package evidence.

Physical-device acceptance on 2026-10-18 paired the Mac QA client with a
separate Windows x86-64 PC using the same dedicated account; the second Mac
client was stopped. Windows received the selected workspace's name, Book icon
and two web tabs, and did not receive the unselected local-only workspace.
Windows then renamed the workspace, added and pinned a web tab, and closed one
original tab. Actual native Sync and UI snapshots on Mac confirmed the new name,
exact URLs, pin state, deletion and exactly two distinct stable tab IDs. The
Windows steps were operated interactively; this is a physical transport/UI
check, separate from the Windows PowerShell diagnostic helper's execution.

Initial setup followed by three cold restarts in a separate, signed-out
production QA profile preserved all three web-tab IDs and one pinned tab,
including two intentionally distinct tabs at the same URL. Launching a restored
profile with an explicit `about:preferences#sync` argument added another
settings tab each time. The QA launcher therefore opens that URL only for a new
profile and resumes existing profiles without an extra URL. No URL-based
deduplication is applied to web tabs. These additional local restart checks do
not claim authenticated multi-device restart coverage.

Normal production package acceptance also passed using locked Runtime
`daily-1155` / `7dd619742b25567e479eeda97ae071d10542ad0b` and Floorp tree
`fbc1e274676fd45f966f30978eef5b5e647c0465`. This used the normal upstream/common
and packaging patches, production before/after-mach asset stages, Gecko's
artifact-mode `mach configure`, `mach build` and `mach package`, the production
macOS omnijar synchronizer and the normal preference override. The final
unsigned DMG was remounted read-only: all 208 Chrome and 168 ESM manifest
entries matched the generated files byte for byte, and all 22 patched Runtime
resources verified. Native engine registration, resource mapping and default
enablement were present. A locally ad-hoc-signed copy launched with a fresh,
unauthenticated profile; without engine-registration or enablement user
overrides it registered/enabled the engine, imported the module and initialized
the workspace UI/providers. This local check used authenticated locked release
assets, not the GitHub runtime-v2 manifest intake/signing/publishing jobs.
Details and the engine hash are recorded in the README. This JavaScript feature
does not require recompiling Gecko's native code. Release signing/notarization
and iOS receiver acceptance remain separate release checks.
