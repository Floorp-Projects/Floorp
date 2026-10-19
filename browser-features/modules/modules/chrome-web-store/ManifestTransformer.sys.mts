/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Manifest Transformer - Converts Chrome manifest.json to Firefox-compatible format
 */

import {
  EXTENSION_ID_SUFFIX,
  GECKO_MIN_VERSION,
  isOptionalPermissionAllowed,
  isPermissionSupported,
  STORE_DOWNLOAD_CLIENT_VERSION,
} from "./Constants.sys.mts";
import type { ChromeManifest, TransformOptions } from "./types.ts";

// =============================================================================
// Constants
// =============================================================================

// =============================================================================
// Types
// =============================================================================

interface TransformResult {
  manifest: ChromeManifest;
  warnings: string[];
  removedPermissions: string[];
}

// =============================================================================
// Public API
// =============================================================================

/**
 * Transform Chrome manifest to Firefox-compatible format
 * @param manifest - Original Chrome manifest
 * @param extensionId - Chrome extension ID
 * @returns Transformed manifest
 */
export function transformManifest(
  manifest: ChromeManifest,
  extensionId: string,
): ChromeManifest {
  const result = transformManifestFull(manifest, { extensionId });
  return result.manifest;
}

/**
 * Transform Chrome manifest with detailed results
 * @param manifest - Original Chrome manifest
 * @param options - Transformation options
 * @returns Transformation result with warnings
 */
export function transformManifestFull(
  manifest: ChromeManifest,
  options: TransformOptions,
): TransformResult {
  const warnings: string[] = [];
  const removedPermissions: string[] = [];

  // Create a deep copy to avoid mutating the original
  const transformed: ChromeManifest = JSON.parse(JSON.stringify(manifest));

  // Add Firefox-specific settings with generated ID
  addGeckoSettings(transformed, options.extensionId, options.minGeckoVersion);

  // Handle Manifest V3 specific transformations
  if (manifest.manifest_version === 3) {
    const bgWarning = transformMV3Background(transformed);
    if (bgWarning) warnings.push(bgWarning);

    transformMV3WebAccessibleResources(transformed);
  }

  // documentId is implemented by Gecko and offscreen is provided by the
  // compatibility runtime. Injecting scripts here would shadow native APIs and
  // tie offscreen documents to the background page's lifetime.

  // Remove unsupported permissions
  const permWarnings = filterUnsupportedPermissions(transformed);
  removedPermissions.push(...permWarnings);

  // Remove Chrome-specific fields
  removeUnsupportedFields(transformed);

  // Sanitize theme properties
  sanitizeTheme(transformed);

  return { manifest: transformed, warnings, removedPermissions };
}

/**
 * Validate that a manifest has required fields
 * @param manifest - Object to validate
 * @returns true if valid manifest structure
 */
export function validateManifest(
  manifest: unknown,
): manifest is ChromeManifest {
  if (!manifest || typeof manifest !== "object") {
    return false;
  }

  const m = manifest as Record<string, unknown>;

  // Required fields
  if (typeof m.manifest_version !== "number") {
    return false;
  }

  if (typeof m.name !== "string" || m.name.length === 0) {
    return false;
  }

  if (typeof m.version !== "string" || m.version.length === 0) {
    return false;
  }

  // Validate manifest version
  if (m.manifest_version !== 2 && m.manifest_version !== 3) {
    return false;
  }

  if (
    m.minimum_chrome_version !== undefined &&
    !parseChromeVersion(m.minimum_chrome_version)
  ) {
    return false;
  }

  return true;
}

/**
 * Check if a manifest is compatible with Firefox
 * @param manifest - Manifest to check
 * @returns Array of compatibility issues (empty if compatible)
 */
export function checkCompatibility(manifest: ChromeManifest): string[] {
  const issues: string[] = [];
  const chromeVersionIssue = checkChromeVersionRequirement(manifest);
  if (chromeVersionIssue) issues.push(chromeVersionIssue);

  // Check for service worker in MV2
  if (manifest.manifest_version === 2 && manifest.background?.service_worker) {
    issues.push("MV2 does not support service workers");
  }

  if (manifest.manifest_version === 3 && manifest.background?.service_worker) {
    issues.push(
      "MV3 service workers use a background event-page fallback; service-worker lifecycle and worker-only APIs are unavailable",
    );
  }

  // Check for unsupported APIs in permissions
  const unsupportedPerms = manifest.permissions?.filter(
    (p) => !isPermissionSupported(p),
  );
  if (unsupportedPerms && unsupportedPerms.length > 0) {
    issues.push(`Unsupported permissions: ${unsupportedPerms.join(", ")}`);
  }

  return issues;
}

/**
 * Bound package requirements to the evaluated Chrome reference. Passing this
 * check is not proof that the extension's APIs are implemented by Floorp.
 */
export function checkChromeVersionRequirement(
  manifest: ChromeManifest,
): string | null {
  const required = manifest.minimum_chrome_version;
  if (required === undefined) return null;
  const minimum = parseChromeVersion(required);
  if (!minimum) {
    return "minimum_chrome_version must contain one to four numeric version components";
  }
  const reference = parseChromeVersion(STORE_DOWNLOAD_CLIENT_VERSION)!;
  for (let index = 0; index < 4; index++) {
    const difference = (minimum[index] ?? 0) - (reference[index] ?? 0);
    if (difference > 0) {
      return `Extension requires Chrome ${
        String(required)
      }; the evaluated Chrome reference is ${STORE_DOWNLOAD_CLIENT_VERSION}. Newer minimum_chrome_version requirements are not supported`;
    }
    if (difference < 0) return null;
  }
  return null;
}

function parseChromeVersion(value: unknown): number[] | null {
  if (typeof value !== "string" || !/^\d+(?:\.\d+){0,3}$/.test(value)) {
    return null;
  }
  const components = value.split(".").map(Number);
  return components.every((component) =>
      Number.isInteger(component) && component <= 0xFFFFFFFF
    )
    ? components
    : null;
}

// =============================================================================
// Internal Transformation Functions
// =============================================================================

/**
 * Add browser_specific_settings.gecko section
 */
function addGeckoSettings(
  manifest: ChromeManifest,
  extensionId: string,
  minVersion?: string,
): void {
  const existing = manifest.browser_specific_settings;
  manifest.browser_specific_settings = {
    ...existing,
    gecko: {
      ...existing?.gecko,
      id: `${extensionId}${EXTENSION_ID_SUFFIX}`,
      strict_min_version: minVersion ?? existing?.gecko?.strict_min_version ??
        GECKO_MIN_VERSION,
      floorp_chrome_extension: {
        schema_version: 1,
        original_id: extensionId,
      },
    },
  };
}

/**
 * Select a document background for MV3 until native worker compatibility is
 * available. Preserve the script kind: Chrome's default is classic, not module.
 * @returns Warning message if conversion was performed
 */
function transformMV3Background(manifest: ChromeManifest): string | null {
  if (!manifest.background?.service_worker) {
    return null;
  }

  const { service_worker: serviceWorker, ...background } = manifest.background;
  const hasDocument = Boolean(background.page || background.scripts?.length);

  // A cross-browser manifest may already include a Firefox document fallback.
  // Prefer it to executing worker source in a document context.
  if (!hasDocument) {
    background.scripts = [serviceWorker];
  }
  manifest.background = background;

  const selection = hasDocument
    ? `Using the existing background document instead of service worker (${serviceWorker})`
    : `Converted service worker (${serviceWorker}) to a ${
      background.type ?? "classic"
    } background event page`;
  return `${selection}; service-worker lifecycle and worker-only APIs are unavailable`;
}

/**
 * Handle MV3 web_accessible_resources format
 * MV3 uses objects with resources/matches, MV2 uses flat string array
 * Also converts Chrome extension_ids to Firefox-compatible format
 */
function transformMV3WebAccessibleResources(manifest: ChromeManifest): void {
  const resources = manifest.web_accessible_resources;

  if (!Array.isArray(resources) || resources.length === 0) {
    return;
  }

  // Check if it's already MV3 format (array of objects)
  const firstEntry = resources[0];
  if (typeof firstEntry !== "object" || !("resources" in firstEntry)) {
    return;
  }

  // Transform extension_ids to Firefox-compatible format
  // Chrome uses bare extension IDs (e.g., "kjchkpkjpiloipaonppkmepcbhcncedo")
  // Firefox requires "*" or addon IDs matching specific patterns:
  // - UUID format: {xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}
  // - Email-like format: name@domain
  for (
    const entry of resources as Array<{
      resources: string[];
      matches?: string[];
      extension_ids?: string[];
    }>
  ) {
    // Filter out chrome-extension:// patterns from matches
    // Firefox doesn't support chrome-extension:// scheme in web_accessible_resources
    if (entry.matches && Array.isArray(entry.matches)) {
      entry.matches = entry.matches.filter(
        (pattern) => !pattern.startsWith("chrome-extension://"),
      );
    }

    if (entry.extension_ids && Array.isArray(entry.extension_ids)) {
      entry.extension_ids = entry.extension_ids.map((id) => {
        // Keep "*" as-is (wildcard, allowed in Firefox)
        if (id === "*") {
          return id;
        }
        // Check if already in Firefox-compatible format
        if (
          id.includes("@") ||
          /^\{[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\}$/i
            .test(
              id,
            )
        ) {
          return id;
        }
        // Convert Chrome extension ID to Firefox format
        return `${id}${EXTENSION_ID_SUFFIX}`;
      });
    }
  }
}

/**
 * Remove Chrome-specific permissions that Firefox doesn't support
 * @returns Array of removed permissions
 */
function filterUnsupportedPermissions(manifest: ChromeManifest): string[] {
  const removed: string[] = [];

  if (manifest.permissions?.length) {
    manifest.permissions = manifest.permissions.filter((permission) => {
      if (!isPermissionSupported(permission)) {
        removed.push(permission);
        return false;
      }
      return true;
    });
  }

  if (manifest.optional_permissions?.length) {
    manifest.optional_permissions = manifest.optional_permissions.filter(
      (permission) => {
        if (!isOptionalPermissionAllowed(permission)) {
          removed.push(`optional:${permission}`);
          return false;
        }
        return true;
      },
    );
  }

  return removed;
}

/**
 * Remove Chrome-specific fields that Firefox doesn't support
 */
function removeUnsupportedFields(manifest: ChromeManifest): void {
  // Remove update_url (Chrome specific)
  if ("update_url" in manifest) {
    delete manifest.update_url;
  }

  // Remove key (used for Chrome extension ID)
  if ("key" in manifest) {
    delete manifest.key;
  }

  // Remove differential_fingerprint (Chrome specific)
  if ("differential_fingerprint" in manifest) {
    delete manifest.differential_fingerprint;
  }
}

/**
 * Sanitize theme properties
 * Some Chrome themes use 0 for ntp_logo_alternate, which Firefox rejects
 */
function sanitizeTheme(manifest: ChromeManifest): void {
  if (!manifest.theme || typeof manifest.theme !== "object") {
    return;
  }

  const theme = manifest.theme as Record<string, unknown>;
  if (!theme.properties || typeof theme.properties !== "object") {
    return;
  }

  const properties = theme.properties as Record<string, unknown>;

  // Fix ntp_logo_alternate: Expected string instead of 0
  if (properties.ntp_logo_alternate === 0) {
    delete properties.ntp_logo_alternate;
  }
}

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * Get the Firefox extension ID that will be generated
 * @param chromeId - Chrome extension ID
 */
export function getFirefoxExtensionId(chromeId: string): string {
  return `${chromeId}${EXTENSION_ID_SUFFIX}`;
}

/**
 * Extract permission hosts from permissions array
 * @param permissions - Permissions array
 */
export function extractHostPermissions(permissions: string[]): string[] {
  return permissions.filter(
    (p) => p.includes("://") || p === "<all_urls>" || p.startsWith("*://"),
  );
}

/**
 * Extract API permissions from permissions array
 * @param permissions - Permissions array
 */
export function extractApiPermissions(permissions: string[]): string[] {
  return permissions.filter(
    (p) => !p.includes("://") && p !== "<all_urls>" && !p.startsWith("*://"),
  );
}
