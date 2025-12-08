# Chrome API Polyfills for Firefox

This directory contains polyfill implementations for Chrome extension APIs that are not natively supported in Firefox. These polyfills are automatically injected during Chrome extension conversion to provide compatibility.

## Purpose

Polyfills in this directory:
- Emulate Chrome-specific APIs using Firefox capabilities
- Are automatically injected during CRX to XPI conversion
- Provide transparent compatibility for extension developers
- Maintain API compatibility with Chrome's implementation

## Available Polyfills

### Offscreen API Polyfill

**File:** `OffscreenPolyfill.sys.mts`  
**Status:** Draft - Not yet integrated  
**Chrome API:** `chrome.offscreen`  
**Investigation:** [docs/investigations/offscreen-api-support.md](../../../../docs/investigations/offscreen-api-support.md)

**Description:**  
Provides compatibility for Chrome's Offscreen API by creating hidden iframes in Firefox MV3 background scripts. The Offscreen API allows extensions to run DOM-based operations in hidden background contexts.

**How it works:**
- Creates hidden iframes to emulate offscreen documents
- Leverages Firefox MV3 background scripts' DOM access
- Provides same API surface as Chrome's Offscreen API
- Enforces same constraints (single document, lifecycle rules)

**Usage:**
```javascript
// This code works in both Chrome and Firefox with the polyfill
await chrome.offscreen.createDocument({
  url: 'offscreen.html',
  reasons: ['DOM_PARSER'],
  justification: 'Parse HTML content'
});

// Use the offscreen document via message passing
// ...

await chrome.offscreen.closeDocument();
```

**Limitations:**
- Only works in contexts with DOM access (background scripts)
- Won't work in pure service worker contexts
- Slight lifecycle differences vs. Chrome
- Performance may differ from native implementation

---

### DocumentId Polyfill

**Directory:** `documentId/`  
**Status:** Draft - Not yet integrated  
**Chrome API:** `documentId` property in `chrome.scripting` and `chrome.tabs`

**Description:**  
Provides compatibility for Chrome's `documentId` property (introduced in Chrome 106+) by generating synthetic documentIds based on `tabId` and `frameId`. Wraps `chrome.scripting.executeScript()` and `chrome.tabs.sendMessage()` to translate documentIds to Firefox-compatible parameters.

**How it works:**
- Generates unique documentIds in format: `floorp-doc-{tabId}-{frameId}-{instanceId}`
- Intercepts API calls with `documentIds` and converts to `tabId`/`frameId` pairs
- Caches parsed documentIds for efficient lookups

**Usage:**
```javascript
// This code works in both Chrome and Firefox with the polyfill
await chrome.scripting.executeScript({
  target: { documentIds: ['floorp-doc-123-0-abc123'] },
  func: () => console.log('Executed!')
});

// With tabs.sendMessage
await chrome.tabs.sendMessage(tabId, message, {
  documentId: 'floorp-doc-123-0-abc123'
});
```

**Limitations:**
- DocumentIds are synthetic and may not persist across browser restarts
- Not fully compatible with all Chrome documentId use cases
- Event handler documentId injection not yet implemented

## Integration with Extension Conversion

### Current State

Polyfills are currently **not integrated** into the conversion pipeline. They are reference implementations showing how compatibility could be achieved.

### Planned Integration

1. **Manifest Transformation** (`ManifestTransformer.sys.mts`):
   - Detect when polyfilled APIs are requested
   - Inject polyfill scripts into background context
   - Mark manifest with polyfill metadata

2. **Build Integration** (`CRXConverter.sys.mts`):
   - Bundle polyfill scripts with converted extension
   - Place polyfills in `__floorp_polyfills__/` directory
   - Ensure polyfills load before extension code

3. **Permission Handling** (`Constants.sys.mts`):
   - Move polyfilled permissions to `POLYFILLED_PERMISSIONS`
   - Remove from `UNSUPPORTED_PERMISSIONS`
   - Allow permission during conversion

## Creating New Polyfills

When creating a new polyfill:

1. **Research** - Create investigation document in `docs/investigations/`
2. **Implementation** - Create polyfill module following this template:
   ```typescript
   /* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
    * This Source Code Form is subject to the terms of the Mozilla Public
    * License, v. 2.0. If a copy of the MPL was not distributed with this
    * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
   
   /**
    * <API Name> Polyfill for Firefox
    *
    * Description...
    * See: docs/investigations/<api-name>-support.md
    */
   
   // Types
   export interface APIInterface {
     // ...
   }
   
   // Implementation
   class APIPolyfillImpl implements APIInterface {
     // ...
   }
   
   // Installation
   export function installAPIPolyfill(): boolean {
     // ...
   }
   ```

3. **Documentation** - Document:
   - What Chrome API it polyfills
   - How it works internally
   - Known limitations
   - Usage examples
   - Testing approach

4. **Testing** - Create tests:
   - Unit tests for polyfill logic
   - Integration tests with sample extensions
   - Real-world extension testing

5. **Integration** - Update:
   - `Constants.sys.mts` - Permission lists
   - `ManifestTransformer.sys.mts` - Injection logic
   - `CRXConverter.sys.mts` - Build integration

## Guidelines

### API Compatibility

- **Match Chrome's API surface** - Same method names, parameters, return types
- **Match Chrome's behavior** - Same constraints, errors, edge cases
- **Document differences** - Clearly note any unavoidable differences

### Performance

- **Minimize overhead** - Polyfills should be lightweight
- **Lazy initialization** - Don't create resources until needed
- **Cleanup** - Properly dispose of resources

### Error Handling

- **Match Chrome's errors** - Same error messages and conditions
- **Fail gracefully** - Provide helpful error messages
- **Don't break extensions** - Errors shouldn't crash the extension

### Code Quality

- **TypeScript** - Use proper type definitions
- **Documentation** - Comprehensive JSDoc comments
- **Testing** - Unit and integration tests
- **Linting** - Follow project code style

## Testing Polyfills

### Unit Tests

Test polyfill logic in isolation:
```typescript
// Test basic functionality
await polyfill.createDocument({ /* ... */ });
assert(await polyfill.hasDocument());
await polyfill.closeDocument();
assert(!(await polyfill.hasDocument()));
```

### Integration Tests

Test with real extensions:
1. Find Chrome extensions using the API
2. Convert using Floorp's CRX converter
3. Install and test functionality
4. Verify API compatibility

### Real-world Testing

Test with popular extensions:
- Extensions with many users
- Extensions using the API heavily
- Extensions with different use cases

## Maintenance

### Monitoring Chrome Changes

- Watch Chrome release notes for API changes
- Update polyfills when Chrome changes behavior
- Test compatibility with new Chrome versions

### Monitoring Firefox Changes

- Track Firefox's MV3 implementation
- Adapt polyfills to Firefox changes
- Watch for new native APIs that could replace polyfills

### Performance Monitoring

- Benchmark polyfill performance
- Compare with native implementation
- Optimize hot paths

## Future Work

### Potential Future Polyfills

Based on extension usage patterns, consider polyfills for:
- `chrome.sidePanel` - Chrome's side panel API
- `chrome.tabCapture` - Tab audio/video capture
- Other highly-requested Chrome APIs

### Native API Migration

If Firefox adds native support for an API:
1. Update polyfill to detect native API
2. Use native API when available
3. Keep polyfill for older Firefox versions
4. Eventually deprecate polyfill

## Resources

- [Chrome Extension APIs](https://developer.chrome.com/docs/extensions/reference/)
- [Firefox WebExtensions API](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions)
- [Browser Extension Polyfills](https://github.com/mozilla/webextension-polyfill)
- [Floorp Investigations](../../../../docs/investigations/)
