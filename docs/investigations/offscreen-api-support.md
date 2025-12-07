# Investigation: Chrome Offscreen API Support in Floorp

**Date:** 2025-12-07  
**Status:** Investigation Complete  
**Priority:** Medium  
**Complexity:** High

## Executive Summary

This document investigates the feasibility of supporting Chrome's Offscreen API in Floorp to improve Chrome extension compatibility. The Offscreen API allows extensions to run DOM-based operations in hidden background contexts, which is crucial for many Chrome extensions that perform tasks like image processing, audio/video manipulation, or DOM parsing without visible UI.

## Background

### What is Chrome's Offscreen API?

The Chrome Offscreen API (introduced in Chrome 109, Manifest V3) allows extensions to create and manage offscreen documents - web pages that run in the background without being visible in browser windows. This API was introduced as part of Chrome's migration from background pages to service workers in MV3.

**Key Use Cases:**
- **DOM Manipulation**: Extensions that need access to DOM APIs (like DOMParser, Canvas, etc.) which aren't available in service workers
- **Media Processing**: Audio/video encoding/decoding, image manipulation
- **Third-party Library Integration**: Running libraries that require a DOM context
- **WebRTC**: Real-time communication features that need DOM APIs
- **IndexedDB Operations**: Some complex database operations work better with a document context

**API Methods:**
```javascript
// Create an offscreen document
chrome.offscreen.createDocument({
  url: 'offscreen.html',
  reasons: ['CLIPBOARD'], // or 'DOM_SCRAPING', 'BLOBS', etc.
  justification: 'Need to parse HTML content'
});

// Close the offscreen document
chrome.offscreen.closeDocument();

// Check if offscreen document exists
chrome.offscreen.hasDocument();
```

**Reasons (enum):**
- `AUDIO_PLAYBACK`: Playing audio
- `BLOBS`: Working with Blob/File APIs
- `CLIPBOARD`: Clipboard operations
- `DOM_PARSER`: Parsing HTML/XML
- `DOM_SCRAPING`: Extracting data from DOM
- `IFRAME_SCRIPTING`: Injecting scripts into iframes
- `LOCAL_STORAGE`: Accessing localStorage
- `MEDIA_STREAM`: Working with media streams
- `OFFSCREEN_CANVAS`: Canvas operations
- `TESTING`: Testing purposes
- `USER_MEDIA`: Camera/microphone access
- `WEB_RTC`: WebRTC operations

## Current State in Floorp

### Current Implementation

Floorp has a robust Chrome extension compatibility layer located in:
- **Module Path**: `browser-features/modules/modules/chrome-web-store/`
- **Key Components**:
  - `ChromeWebStore.sys.mts`: Main module for Chrome extension support
  - `ManifestTransformer.sys.mts`: Converts Chrome manifest to Firefox format
  - `Constants.sys.mts`: Permission lists and configuration
  - `CRXConverter.sys.mts`: Converts CRX files to XPI
  - `CodeValidator.sys.mts`: Validates compatibility

### Current Status of Offscreen API

**Location**: `browser-features/modules/modules/chrome-web-store/Constants.sys.mts:95`

```typescript
export const UNSUPPORTED_PERMISSIONS = [
  // ... other permissions
  "offscreen",  // Currently marked as unsupported
  // ... other permissions
] as const;
```

**Impact**: Extensions requesting the `offscreen` permission will have it stripped during conversion, causing functionality issues for extensions that depend on it.

## Firefox/Gecko API Comparison

### Firefox Equivalent APIs

Firefox does **not** have a direct equivalent to Chrome's Offscreen API. However, there are several related APIs and approaches:

#### 1. Background Pages (Deprecated in MV3)
```javascript
// manifest.json (MV2)
{
  "background": {
    "page": "background.html",
    "persistent": false
  }
}
```

**Status**: 
- ✅ Available in MV2
- ❌ Not available in MV3 (service workers only)
- ⚠️ Different from Offscreen API (always loaded, not on-demand)

#### 2. Hidden iFrames in Background Context
Extensions can create hidden iframes within their background page to achieve similar functionality:

```javascript
// In background script
function createHiddenFrame(url) {
  const iframe = document.createElement('iframe');
  iframe.style.display = 'none';
  iframe.src = browser.runtime.getURL(url);
  document.body.appendChild(iframe);
  return iframe;
}
```

**Limitations**:
- Only works with MV2 background pages
- Not available in MV3 service workers
- Limited lifecycle control

#### 3. Web Workers
```javascript
const worker = new Worker('worker.js');
```

**Limitations**:
- No DOM access (main limitation)
- Can't use most Web APIs
- Not a replacement for Offscreen API

#### 4. Side Effects Approach
Some functionality can be moved to content scripts or popup contexts, but this is not always feasible.

### Firefox MV3 Status

Firefox's MV3 implementation (as of Firefox 128+):
- ✅ Supports Event Pages (similar to MV2 background pages)
- ✅ Background scripts with DOM access
- ❌ No Offscreen API
- ⚠️ Different from Chrome's pure service worker approach

**Key Difference**: Firefox MV3 background scripts have DOM access, making Offscreen API less critical than in Chrome.

## ChromeXPIPorter Investigation

### About ChromeXPIPorter

ChromeXPIPorter is a tool/library by @FoxRefire that converts Chrome extensions (CRX format) to Firefox extensions (XPI format). It handles manifest transformation and API compatibility.

### Potential Approaches (Inferred)

Since external access is blocked, based on common patterns for such tools, ChromeXPIPorter likely handles Offscreen API through:

#### Approach 1: Polyfill Using Background Page
```javascript
// Polyfill chrome.offscreen API
if (!chrome.offscreen) {
  chrome.offscreen = {
    createDocument: async ({ url, reasons, justification }) => {
      // Create hidden iframe in background page
      const iframe = document.createElement('iframe');
      iframe.style.display = 'none';
      iframe.src = browser.runtime.getURL(url);
      document.body.appendChild(iframe);
      // Store reference for cleanup
      window.__offscreenDocument = iframe;
    },
    
    closeDocument: async () => {
      if (window.__offscreenDocument) {
        window.__offscreenDocument.remove();
        window.__offscreenDocument = null;
      }
    },
    
    hasDocument: async () => {
      return !!window.__offscreenDocument;
    }
  };
}
```

#### Approach 2: Manifest Transformation
Transform MV3 manifest to use Firefox's event page pattern:

```javascript
// Chrome MV3
{
  "manifest_version": 3,
  "background": {
    "service_worker": "background.js"
  },
  "permissions": ["offscreen"]
}

// Transform to Firefox MV3
{
  "manifest_version": 3,
  "background": {
    "scripts": ["background.js"],
    "type": "module"
  }
  // Remove "offscreen" permission
}
```

#### Approach 3: Code Transformation
Automatically rewrite code that uses Offscreen API:

```javascript
// Original Chrome code
await chrome.offscreen.createDocument({
  url: 'offscreen.html',
  reasons: ['DOM_PARSER'],
  justification: 'Parse HTML'
});

// Transformed to Firefox equivalent
const iframe = document.createElement('iframe');
iframe.src = browser.runtime.getURL('offscreen.html');
iframe.style.display = 'none';
document.body.appendChild(iframe);
```

## Proposed Implementation for Floorp

### Option 1: Polyfill Library (Recommended)

**Approach**: Inject a polyfill script that emulates Offscreen API using Firefox's capabilities.

**Implementation Steps**:

1. **Create Offscreen Polyfill Module**
   - Location: `browser-features/modules/modules/chrome-web-store/polyfills/OffscreenPolyfill.sys.mts`
   - Implement polyfill that creates hidden iframes in background context

2. **Update Manifest Transformer**
   - Keep `offscreen` permission instead of removing it
   - Inject polyfill script into background context
   - Add metadata to indicate polyfilled APIs

3. **Update Constants**
   - Remove `"offscreen"` from `UNSUPPORTED_PERMISSIONS`
   - Add to new `POLYFILLED_PERMISSIONS` array

**Pros**:
- ✅ Transparent to extension developers
- ✅ Works with existing Chrome extensions
- ✅ No manual code changes needed
- ✅ Provides similar API surface

**Cons**:
- ⚠️ Only works for MV2 or Firefox's MV3 (with background scripts)
- ⚠️ Slight behavior differences vs. Chrome
- ⚠️ Lifecycle management complexity

**Estimated Effort**: Medium (2-3 weeks)

### Option 2: Code Transformation

**Approach**: Automatically rewrite extension code that uses `chrome.offscreen` API calls.

**Pros**:
- ✅ More control over output
- ✅ Can optimize for Firefox patterns

**Cons**:
- ❌ Complex AST parsing required
- ❌ May break with minified code
- ❌ Hard to maintain
- ❌ Risk of breaking extensions

**Estimated Effort**: High (4-6 weeks)

**Status**: Not Recommended

### Option 3: Documentation + Manual Handling

**Approach**: Document the limitation and provide migration guide.

**Pros**:
- ✅ Minimal implementation effort
- ✅ No risk of bugs

**Cons**:
- ❌ Poor user experience
- ❌ Reduced compatibility
- ❌ Extensions won't work out of the box

**Estimated Effort**: Low (1-2 days)

**Status**: Fallback Option Only

## Technical Implementation Details

### Polyfill Implementation (Option 1 - Recommended)

#### 1. Create Polyfill Module

```typescript
// browser-features/modules/modules/chrome-web-store/polyfills/OffscreenPolyfill.sys.mts

/**
 * Polyfill for Chrome Offscreen API
 * Provides compatibility layer for Firefox by using hidden iframes
 */

interface OffscreenDocumentOptions {
  url: string;
  reasons: OffscreenReason[];
  justification: string;
}

type OffscreenReason =
  | 'AUDIO_PLAYBACK'
  | 'BLOBS'
  | 'CLIPBOARD'
  | 'DOM_PARSER'
  | 'DOM_SCRAPING'
  | 'IFRAME_SCRIPTING'
  | 'LOCAL_STORAGE'
  | 'MEDIA_STREAM'
  | 'OFFSCREEN_CANVAS'
  | 'TESTING'
  | 'USER_MEDIA'
  | 'WEB_RTC';

class OffscreenPolyfill {
  private offscreenFrame: HTMLIFrameElement | null = null;
  
  async createDocument(options: OffscreenDocumentOptions): Promise<void> {
    // Check if document already exists
    if (this.offscreenFrame) {
      throw new Error('Only one offscreen document may exist');
    }
    
    // Validate reasons
    if (!options.reasons || options.reasons.length === 0) {
      throw new Error('Must specify at least one reason');
    }
    
    // Create hidden iframe
    const iframe = document.createElement('iframe');
    iframe.style.display = 'none';
    iframe.style.position = 'absolute';
    iframe.style.left = '-9999px';
    iframe.src = browser.runtime.getURL(options.url);
    
    // Add to document
    document.body.appendChild(iframe);
    this.offscreenFrame = iframe;
    
    console.log('[Offscreen Polyfill] Created offscreen document:', options);
  }
  
  async closeDocument(): Promise<void> {
    if (this.offscreenFrame) {
      this.offscreenFrame.remove();
      this.offscreenFrame = null;
      console.log('[Offscreen Polyfill] Closed offscreen document');
    }
  }
  
  async hasDocument(): Promise<boolean> {
    return this.offscreenFrame !== null;
  }
}

// Install polyfill
export function installOffscreenPolyfill(): void {
  if (typeof chrome === 'undefined' || chrome.offscreen) {
    // Already exists or not in extension context
    return;
  }
  
  const polyfill = new OffscreenPolyfill();
  
  // @ts-ignore - Polyfilling Chrome API
  chrome.offscreen = {
    createDocument: polyfill.createDocument.bind(polyfill),
    closeDocument: polyfill.closeDocument.bind(polyfill),
    hasDocument: polyfill.hasDocument.bind(polyfill),
  };
  
  console.log('[Offscreen Polyfill] Installed');
}
```

#### 2. Update Manifest Transformer

```typescript
// browser-features/modules/modules/chrome-web-store/ManifestTransformer.sys.mts

function injectPolyfills(manifest: ChromeManifest): string[] {
  const injectedPolyfills: string[] = [];
  
  // Check if offscreen permission is requested
  if (manifest.permissions?.includes('offscreen')) {
    // Inject polyfill script
    if (manifest.background) {
      if (manifest.background.scripts) {
        // Prepend polyfill to background scripts
        manifest.background.scripts.unshift('__floorp_polyfills__/offscreen.js');
        injectedPolyfills.push('offscreen');
      } else if (manifest.background.service_worker) {
        // For service workers, wrap with polyfill
        // Note: This won't work well in pure service workers
        console.warn('Offscreen API polyfill has limited support with service workers');
      }
    }
  }
  
  return injectedPolyfills;
}

// Update transformManifestFull to call injectPolyfills
export function transformManifestFull(
  manifest: ChromeManifest,
  options: TransformOptions,
): TransformResult {
  const warnings: string[] = [];
  const removedPermissions: string[] = [];
  const transformed: ChromeManifest = JSON.parse(JSON.stringify(manifest));
  
  // ... existing code ...
  
  // Inject polyfills before removing permissions
  const injectedPolyfills = injectPolyfills(transformed);
  if (injectedPolyfills.length > 0) {
    warnings.push(`Injected polyfills: ${injectedPolyfills.join(', ')}`);
  }
  
  // ... rest of transformations ...
  
  return { manifest: transformed, warnings, removedPermissions };
}
```

#### 3. Update Constants

```typescript
// browser-features/modules/modules/chrome-web-store/Constants.sys.mts

/**
 * Permissions that are polyfilled for compatibility
 */
export const POLYFILLED_PERMISSIONS = [
  'offscreen',
] as const;

/**
 * Chrome-specific permissions that Firefox doesn't support
 * (Remove 'offscreen' from this list)
 */
export const UNSUPPORTED_PERMISSIONS = [
  // ... other permissions (remove 'offscreen') ...
  "processes",
  "sidePanel",
  "signedInDevices",
  "tabCapture",
  // ... rest ...
] as const;

/**
 * Check if a permission is supported or can be polyfilled
 */
export function isPermissionSupportedOrPolyfilled(permission: string): boolean {
  return isPermissionSupported(permission) || 
         POLYFILLED_PERMISSIONS.includes(permission as any);
}
```

#### 4. Create Build Integration

Need to ensure the polyfill is bundled with converted extensions:

```typescript
// browser-features/modules/modules/chrome-web-store/CRXConverter.sys.mts

async function bundlePolyfills(outputDir: string, polyfills: string[]): Promise<void> {
  const polyfillDir = PathUtils.join(outputDir, '__floorp_polyfills__');
  await IOUtils.makeDirectory(polyfillDir, { ignoreExisting: true });
  
  for (const polyfill of polyfills) {
    if (polyfill === 'offscreen') {
      // Copy offscreen polyfill
      const polyfillPath = PathUtils.join(polyfillDir, 'offscreen.js');
      const polyfillContent = getOffscreenPolyfillCode();
      await IOUtils.writeUTF8(polyfillPath, polyfillContent);
    }
  }
}

function getOffscreenPolyfillCode(): string {
  // Return the compiled polyfill code
  return `
    (function() {
      'use strict';
      
      // Offscreen Polyfill Implementation
      // ... (insert compiled polyfill code) ...
      
    })();
  `;
}
```

### Limitations and Caveats

#### Known Limitations

1. **Service Worker Incompatibility**
   - Chrome's pure service workers (no DOM) → Firefox's event pages (with DOM)
   - Polyfill won't work in true service worker contexts
   - Mitigation: Firefox MV3 provides DOM in background scripts

2. **Lifecycle Differences**
   - Chrome's offscreen documents have specific lifecycle rules
   - Firefox iframes may behave slightly differently
   - Mitigation: Document behavior differences

3. **Performance**
   - Hidden iframes have some memory overhead
   - May not be as optimized as native implementation
   - Mitigation: Acceptable for most use cases

4. **Singleton Constraint**
   - Chrome allows only one offscreen document at a time
   - Polyfill enforces same constraint
   - Both implementations: ✅ Compatible

5. **Messaging**
   - Inter-context communication works differently
   - Chrome: `chrome.runtime.sendMessage` between contexts
   - Firefox: Same API should work
   - Both implementations: ✅ Compatible

#### Testing Requirements

1. **Basic Functionality Tests**
   - Create offscreen document
   - Close offscreen document
   - Check document existence
   - Multiple creation attempts (should fail)

2. **API Usage Tests**
   - DOM manipulation in offscreen context
   - Canvas operations
   - Clipboard access
   - Message passing

3. **Real Extension Tests**
   - Find Chrome extensions using Offscreen API
   - Test conversion and installation
   - Verify functionality

## Migration Path for Existing Extensions

### For Extension Developers

If manually porting extensions, developers should be aware:

#### Chrome MV3 with Offscreen API
```javascript
// background.js (service worker)
chrome.offscreen.createDocument({
  url: 'offscreen.html',
  reasons: ['DOM_PARSER'],
  justification: 'Parse HTML'
});
```

#### Firefox MV3 Alternative
```javascript
// background.js (with DOM access)
// No offscreen needed! Can use DOM directly:
const parser = new DOMParser();
const doc = parser.parseFromString(html, 'text/html');
```

### With Floorp's Polyfill
```javascript
// Same code works in both browsers!
chrome.offscreen.createDocument({
  url: 'offscreen.html',
  reasons: ['DOM_PARSER'],
  justification: 'Parse HTML'
});
// Polyfill handles it automatically in Firefox
```

## Recommendations

### Immediate Actions (Phase 1)

1. **✅ Implement Offscreen API Polyfill** (Option 1)
   - Create polyfill module
   - Update manifest transformer
   - Update constants
   - Add build integration

2. **📝 Document Polyfill Behavior**
   - Create user-facing documentation
   - Document known limitations
   - Provide migration examples

3. **🧪 Create Test Suite**
   - Unit tests for polyfill
   - Integration tests with sample extensions
   - Real-world extension testing

### Future Enhancements (Phase 2)

1. **🔍 Monitor Firefox Development**
   - Track Firefox's MV3 implementation
   - Watch for potential native Offscreen API
   - Adapt polyfill as needed

2. **🎯 Improve Polyfill**
   - Add better lifecycle management
   - Improve error handling
   - Performance optimizations

3. **📊 Collect Metrics**
   - Track which extensions use Offscreen API
   - Measure polyfill performance
   - Gather user feedback

### Long-term Strategy (Phase 3)

1. **🤝 Contribute to Firefox**
   - Propose Offscreen API for Firefox
   - Share implementation experience
   - Help standardize the API

2. **🔄 Maintain Compatibility**
   - Keep polyfill updated with Chrome changes
   - Ensure continued Firefox compatibility
   - Support new use cases as they emerge

## Risk Assessment

| Risk | Severity | Likelihood | Mitigation |
|------|----------|------------|------------|
| Polyfill breaks existing extensions | High | Low | Thorough testing, gradual rollout |
| Performance issues | Medium | Low | Benchmark, optimize as needed |
| Chrome API changes | Medium | Medium | Monitor Chrome releases, update polyfill |
| Firefox MV3 changes | Medium | Medium | Track Firefox development |
| Maintenance burden | Medium | Medium | Good documentation, clean code |
| User confusion | Low | Low | Clear documentation |

## Success Metrics

- ✅ Polyfill successfully implements core Offscreen API
- ✅ At least 80% of extensions using Offscreen API work correctly
- ✅ No significant performance degradation
- ✅ Positive user feedback
- ✅ Reduced extension compatibility complaints

## References

### Official Documentation

- [Chrome Offscreen API Documentation](https://developer.chrome.com/docs/extensions/reference/api/offscreen)
- [Firefox WebExtensions API](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions)
- [Firefox Manifest V3 Migration](https://extensionworkshop.com/documentation/develop/manifest-v3-migration-guide/)

### Related Work

- ChromeXPIPorter by @FoxRefire (GitHub: FoxRefire/ChromeXPIPorter)
- Mozilla's webextension-polyfill
- Browser Extension API compatibility tables

### Internal References

- `browser-features/modules/modules/chrome-web-store/` - Chrome extension support
- `browser-features/modules/modules/chrome-web-store/Constants.sys.mts:95` - Permission lists
- `browser-features/modules/modules/chrome-web-store/ManifestTransformer.sys.mts` - Manifest transformation

## Conclusion

Implementing Chrome Offscreen API support in Floorp is **feasible and recommended** through a polyfill approach. While Firefox doesn't have a native equivalent, the polyfill can provide sufficient compatibility for most use cases by leveraging hidden iframes in background contexts.

The recommended implementation (Option 1: Polyfill Library) provides the best balance of:
- ✅ Compatibility with existing Chrome extensions
- ✅ Transparent developer experience
- ✅ Reasonable implementation effort
- ✅ Maintainability

Given Firefox's different approach to MV3 (background scripts with DOM access vs. pure service workers), many extensions may not even need the Offscreen API in Firefox. However, providing the polyfill ensures maximum compatibility for extensions ported from Chrome.

**Recommended Next Steps:**
1. Approve implementation approach
2. Create detailed technical specification
3. Implement polyfill module (Sprint 1)
4. Update manifest transformer (Sprint 1)
5. Create test suite (Sprint 2)
6. Beta test with real extensions (Sprint 2)
7. Document and release (Sprint 3)

**Timeline Estimate:** 3-4 weeks for full implementation and testing
**Resources Required:** 1 developer, part-time QA support
**Priority:** Medium (improves compatibility, not blocking)
