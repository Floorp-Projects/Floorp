# Workspace and tab Sync v1

Status: implementation contract, pending two-device FxA acceptance testing.

Floorp Desktop uses Firefox Sync's authenticated, encrypted transport for an
independent `floorp-workspaces` collection. The engine version is `1`; its switch
is `services.sync.engine.floorp-workspaces`. It is enabled by default, but does
not run while `floorp.workspaces.enabled` is false. Signing into a Mozilla
Account and enabling Firefox Sync are prerequisites for transport.

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
and tab IDs are distinct, random UUID strings, globally unique in the collection.
Preserve received IDs exactly; new implementations should generate lowercase
UUIDs. Moving a tab to another workspace or explicitly reopening a closed tab
uses a new tab ID. Session restoration alone must not revive a retired ID.

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
cleartext is `{ "id": "…", "deleted": true }`, with no `entity` required.
There is no custom REST client, token storage, encryption scheme, or account
system. Application Services clients must use the normal collection keys and
Sync `meta/global` engine contract, not upload the inner `entity` unwrapped.

`modifiedAt` is a non-negative safe integer in Unix milliseconds. A local edit
advances beyond both the wall clock and every clock already observed in that
workspace. `modifiedBy` is a persisted random device UUID, not an account token
or FxA device identifier. Metadata and each tab merge independently by
`(modifiedAt, modifiedBy)`. Equal clock/device conflicts use the lexically larger
JSON value with the canonical field order produced by the TypeScript parser;
receivers should reproduce that order and standard JSON escaping. UUID ordering
is case-sensitive. Duplicate-ID records and workspace ownership changes for an
existing tab ID are rejected.

Deletion wins over every live version of the same UUID, including an offline
client's newer navigation or changed position. Closed-tab payloads are removed
from the profile cache only after the corresponding standard tombstone is
successfully committed. A lightweight, account-bound retired UUID set remains
locally, preventing stale records or restored sessions from reviving those IDs.
The set contains IDs only, and does not consume wire payload space. There is no
cumulative tab-count limit or monolithic workspace payload.

Tabs may arrive before metadata; they are staged without opening a placeholder
workspace. The first actual metadata record supplies its name and icon even
when its valid clock is zero. Subsequent metadata records use the normal merge.
A parent tombstone suppresses its later metadata and child records regardless of
arrival order, including on a client that never saw that workspace before.

## Tab content and order

Only HTTP(S) URLs without embedded credentials are synchronized. Private windows,
local files, privileged/internal pages, extension URLs, forms, cookies, history,
SessionStore state and local container numbers are excluded. Names and icons are
at most 256 UTF-16 code units, titles 1024, and URLs 8192. Strings must be valid
Unicode. Desktop truncates long titles without leaving unpaired surrogates.

Each canonical inner entity has a 16 KiB UTF-8 ceiling, leaving ample room for
CryptoWrapper's ID, encryption padding, base64 expansion and BSO metadata under
Firefox's normal 256 KiB record limit. The wire helpers validate outgoing and
incoming entities before they are saved or used to navigate.

Container assignment remains local. A received tab uses that workspace's existing
local default container; a newly discovered workspace uses container 0. The
sender's numeric container ID is never interpreted on another installation.

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

Incoming tab closures are persisted and deferred until all incoming batches and
backfill have succeeded. A parent deletion then cancels its children's deferred
UI closures, retaining the local workspace. Unresolved decrypt/reconcile/apply
failures are independently persisted and retried even after Firefox's usual
repeated-failure list would drop them. Pending closures remain deferred across
restart; capture excludes their retired IDs.

If a failed server record is physically removed rather than repaired, this
conservative policy can keep deferred closures pending. Turning off workspace
sync preserves local content; disconnecting Firefox Sync clears the account's
workspace cache and subscriptions, after which the user can reconnect and opt in
again. This reset is intentional and does not delete local browser content.

`weave:service:start-over` disarms the cached account state. In addition, every
engine startup checks the signed-in FxA UID against the stored owner before
uploading. A different account starts with local-only workspaces and no inherited
remote subscriptions or retired IDs.

Invalid local metadata pauses only that workspace and retains its last good
record. Its menu shows an attention state and a localized explanation; failed
initial selection rolls back and displays an alert. Other workspaces continue
syncing. Correcting the metadata resumes capture and requests a fresh download.

## Build paths and validation

Production registration is in
`.github/patches/floorp-runtime/common/workspace-sync.patch`. `package.yml` applies
all common source patches before packaging even when native Runtime artifacts
are reused; this JavaScript-only patch needs no Gecko native recompilation.
Development patches extracted Runtime code using
`tools/patches/modules-services-sync-service.sys.patch`. The host compatibility
test asserts that these changes are identical after path mapping.

The pure model and wire tests are browser tests under
`browser-features/modules/test/workspaces/`. They cover concurrent additions,
offline close plus reorder, clock regression/ties, payload and Unicode boundaries,
per-tab entities, and large aggregates. Implementation validation also exercised
the native Sync engine/record APIs and encryption in an isolated Gecko 158
profile. A patched full Runtime build and authenticated two-device FxA transport
have not yet been exercised; those remain release acceptance checks.
