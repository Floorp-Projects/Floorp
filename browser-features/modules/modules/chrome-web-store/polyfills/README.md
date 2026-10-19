# Chrome Extension Compatibility Entry Points

This directory retains legacy module interfaces for offscreen documents and
document IDs. Converted extensions use the native Floorp Runtime implementations
described below; conversion no longer injects these scripts into backgrounds.

## Purpose

Native implementations can share Gecko document identities and extension
lifecycle management across backgrounds, extension pages, and message senders.

## Available Polyfills

### Native Offscreen Documents

Converted MV3 extensions with the `offscreen` permission use a dedicated Runtime
host through `chrome.offscreen.createDocument`, `closeDocument`, and
`hasDocument`. The host belongs to the extension and survives event background
termination. `runtime.getContexts` exposes the real document as
`OFFSCREEN_DOCUMENT`, with the same native document ID used by messaging.

The Runtime validates extension ownership of the URL, reserves the single host
before asynchronous loading, and cleans up cancelled or failed loads, closed
documents, and extension shutdown. Offscreen documents and their extension
iframes receive the runtime API. An `AUDIO_PLAYBACK` reason starts a 30 second
silence timeout driven by Gecko's media controller.

**Usage:**
```javascript
// Available in converted MV3 extensions using the patched Floorp Runtime.
await chrome.offscreen.createDocument({
  url: 'offscreen.html',
  reasons: ['DOM_PARSER'],
  justification: 'Parse HTML content'
});

// Use the offscreen document via message passing
// ...

await chrome.offscreen.closeDocument();
```

The former iframe polyfill is retired. Its module remains as a bridge to an
already available native API, and its installer and generated script leave API
bindings unchanged. Existing converted packages should be reconverted to remove
their bundled legacy script. A separate native tab-capture API provides
document-bound, single-use stream IDs and real video-only recording, including
Offscreen consumers. Audio, desktop capture, and `consumerTabId` are unsupported
and reject explicitly. The capture API requires an actual extension action
invocation; hover/preload and generic `activeTab` grants do not authorize it.

---

### Native Document IDs

The pinned Floorp Runtime provides document IDs through Gecko's
`ExtensionDocumentId` module. `webNavigation`, `scripting`, message senders,
`tabs.sendMessage`, `tabs.connect`, and `runtime.getContexts` share UUIDs tied to
real inner windows. Scripts target a document within the requested tab; an ID
from a previous navigation cannot select the replacement document in that frame.

Converted extensions use these native APIs directly. No documentId script or
extra permission is needed. IDs must be treated as opaque values; they cannot be
reconstructed from a tab or frame ID. The former synthetic ID implementation has
been retired, and its exported installer and generated script are inert
compatibility entry points. Already installed converted packages need to be
reconverted to replace their bundled legacy script.

```javascript
const [{ documentId }] = await chrome.scripting.executeScript({
  target: { tabId },
  func: () => document.title,
});
await chrome.tabs.sendMessage(tabId, message, { documentId });
```

## Integration with Extension Conversion

`ManifestTransformer.sys.mts` preserves classic and module background script types
and adds extension-specific compatibility metadata. `CRXConverter.sys.mts`
retains the original manifest and a conversion report. The Runtime checks that
the original Chrome ID agrees with the installed extension ID before exposing
compatibility APIs. Source patches are maintained under
`.github/patches/floorp-runtime/common/`, with matching extracted artifact patches
under `tools/patches/` for browser integration tests.

## Verification

Run the host lifecycle and patch parity tests from the repository root:

```sh
deno test --allow-read tools/src/offscreen_runtime_patch.test.ts
```

After applying the artifact patches to a built browser, run the actual native
extension tests in a disposable profile:

```sh
python3 tools/chrome-extension-runtime/run-browser-tests.py \
  --browser _dist/bin/floorp \
  --test browser-features/modules/modules/chrome-web-store/test/polyfills/offscreen/OffscreenNative.test.mts \
  --test browser-features/modules/modules/chrome-web-store/test/polyfills/offscreen/OffscreenPolyfill.test.mts
```

The native test installs a temporary extension, verifies offscreen and iframe
API restrictions, checks context filters and document IDs, stops the background,
and confirms messaging and Ports still work through the surviving offscreen
host. It also verifies immediate close, recreation, and extension shutdown.
The source patch includes an xpcshell lifecycle regression for source-built
Runtimes. Capture streams and platform audio routing require separate tests.

## Adding Compatibility APIs

Choose the native Runtime layer when behavior depends on extension lifetime,
permissions, document identity, or process boundaries. Keep source and extracted
artifact patches equivalent and validate against `floorp-runtime.lock.json` in
an isolated fixture. Restrict compatibility changes to the intended extension
mode and use actual extension tests to verify bindings and event delivery.
Document supported behavior and remaining gaps alongside the conversion report.

## Resources

- [Chrome Extension APIs](https://developer.chrome.com/docs/extensions/reference/)
- [Firefox WebExtensions API](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions)
- [Browser Extension Polyfills](https://github.com/mozilla/webextension-polyfill)
- [Floorp Investigations](../../../../docs/investigations/)
