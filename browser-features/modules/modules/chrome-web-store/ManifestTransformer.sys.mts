/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Manifest Transformer - Converts Chrome manifest.json to Firefox-compatible format
 */

import {
  GECKO_MIN_VERSION,
  EXTENSION_ID_SUFFIX,
  UNSUPPORTED_PERMISSIONS,
} from "./Constants.sys.mts";
import type { ChromeManifest } from "./types.ts";

/**
 * Transform Chrome manifest to Firefox-compatible format
 */
export function transformManifest(
  manifest: ChromeManifest,
  extensionId: string,
): ChromeManifest {
  // Create a shallow copy to avoid mutating the original
  const transformed: ChromeManifest = { ...manifest };

  // Add Firefox-specific settings with generated ID
  addGeckoSettings(transformed, extensionId);

  // Handle Manifest V3 specific transformations
  if (manifest.manifest_version === 3) {
    transformMV3Background(transformed);
    // Firefox supports host_permissions in MV3, so we don't need to merge them
    // transformMV3HostPermissions(transformed);
    transformMV3WebAccessibleResources(transformed);
  }

  // Remove unsupported permissions
  filterUnsupportedPermissions(transformed);

  // Remove update_url if present (Chrome specific)
  if ("update_url" in transformed) {
    delete transformed.update_url;
  }

  console.log(
    "[ManifestTransformer] Transformed:",
    transformed.name,
    "v" + transformed.version,
  );

  return transformed;
}

/**
 * Add browser_specific_settings.gecko section
 */
function addGeckoSettings(manifest: ChromeManifest, extensionId: string): void {
  manifest.browser_specific_settings = {
    gecko: {
      id: `${extensionId}${EXTENSION_ID_SUFFIX}`,
      strict_min_version: GECKO_MIN_VERSION,
    },
  };
}

/**
 * Convert MV3 service_worker to background scripts
 * Firefox doesn't support service_worker in the same way as Chrome
 */
function transformMV3Background(manifest: ChromeManifest): void {
  if (!manifest.background?.service_worker) {
    return;
  }

  console.log(
    "[ManifestTransformer] Converting service_worker to background scripts",
  );

  manifest.background = {
    scripts: [manifest.background.service_worker],
    type: "module",
  };
}

/**
 * Handle MV3 web_accessible_resources format
 * MV3 uses objects with resources/matches, MV2 uses flat string array
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

  // Firefox 101+ supports MV3 web_accessible_resources format
  // We keep it as-is for modern Firefox versions
  // For older versions, we would need to flatten it:
  //
  // const flatResources: string[] = [];
  // for (const entry of resources as WebAccessibleResourceEntry[]) {
  //   if (entry.resources) {
  //     flatResources.push(...entry.resources);
  //   }
  // }
  // manifest.web_accessible_resources = flatResources;

  console.log(
    "[ManifestTransformer] Keeping MV3 web_accessible_resources format",
  );
}

/**
 * Remove Chrome-specific permissions that Firefox doesn't support
 */
function filterUnsupportedPermissions(manifest: ChromeManifest): void {
  const unsupportedSet = new Set<string>(UNSUPPORTED_PERMISSIONS);

  if (manifest.permissions?.length) {
    const originalCount = manifest.permissions.length;
    manifest.permissions = manifest.permissions.filter(
      (permission) => !unsupportedSet.has(permission),
    );

    const removedCount = originalCount - manifest.permissions.length;
    if (removedCount > 0) {
      console.log(
        `[ManifestTransformer] Removed ${removedCount} unsupported permission(s)`,
      );
    }
  }

  if (manifest.optional_permissions?.length) {
    const originalCount = manifest.optional_permissions.length;
    manifest.optional_permissions = manifest.optional_permissions.filter(
      (permission) => {
        // contextMenus is not supported in optional_permissions in Firefox
        if (permission === "contextMenus") {
          return false;
        }
        return !unsupportedSet.has(permission);
      },
    );

    const removedCount = originalCount - manifest.optional_permissions.length;
    if (removedCount > 0) {
      console.log(
        `[ManifestTransformer] Removed ${removedCount} unsupported optional_permission(s)`,
      );
    }
  }
}

/**
 * Validate that a manifest has required fields
 */
export function validateManifest(
  manifest: unknown,
): manifest is ChromeManifest {
  if (!manifest || typeof manifest !== "object") {
    return false;
  }

  const m = manifest as Record<string, unknown>;

  return (
    typeof m.manifest_version === "number" &&
    typeof m.name === "string" &&
    typeof m.version === "string"
  );
}
