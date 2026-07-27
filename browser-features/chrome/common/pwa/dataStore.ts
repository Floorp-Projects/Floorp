/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { buildSsbKey, parseSsbKey } from "#libs/pwa/ssbKeyUtils.ts";
import type { LegacyPWAEntry, Manifest } from "./type.ts";

export class DataManager {
  private get ssbStoreFile() {
    return PathUtils.join(PathUtils.profileDir, "ssb", "ssb.json");
  }

  private get ssbStoreDirectory() {
    return PathUtils.join(PathUtils.profileDir, "ssb");
  }

  constructor() {
  }

  /**
   * Builds a composite key from start_url and userContextId.
   * Format: "start_url:userContextId"
   */
  public static buildKey(startUrl: string, userContextId: number = 0): string {
    return buildSsbKey(startUrl, userContextId);
  }

  /**
   * Parses a composite key back into start_url and userContextId.
   */
  public static parseKey(
    key: string,
  ): { startUrl: string; userContextId: number } {
    return parseSsbKey(key);
  }

  public async getCurrentSsbData(): Promise<Record<string, Manifest>> {
    await this.ensureStoreDirectory();
    const fileExists = await IOUtils.exists(this.ssbStoreFile);
    if (!fileExists) {
      await IOUtils.writeJSON(this.ssbStoreFile, {});
      return {};
    }

    const data = await IOUtils.readJSON(this.ssbStoreFile);

    // Check if this is legacy format and migrate if needed
    if (this.isLegacyFormat(data)) {
      const migratedData = this.migrateFromLegacyFormat(
        data as Record<string, LegacyPWAEntry>,
      );
      await this.overrideCurrentSsbData(migratedData);
      return migratedData;
    }

    // Migrate old key format (start_url only) to composite key (start_url:userContextId)
    if (this.isOldKeyFormat(data as Record<string, Manifest>)) {
      const migratedData = this.migrateToCompositeKeyFormat(
        data as Record<string, Manifest>,
      );
      await this.overrideCurrentSsbData(migratedData);
      return migratedData;
    }

    return data as Record<string, Manifest>;
  }

  // deno-lint-ignore no-explicit-any
  private isLegacyFormat(data: any): boolean {
    // Check if the data has the structure of legacy format
    // In legacy format, entries have 'startURI' and 'manifest' properties
    if (!data) return false;

    for (const key in data) {
      if (data[key].startURI && data[key].manifest) {
        return true;
      }
    }
    return false;
  }

  /**
   * Checks if the data uses the old key format (start_url only, no composite key).
   * Returns true if any key does not contain a userContextId suffix.
   */
  private isOldKeyFormat(data: Record<string, Manifest>): boolean {
    if (!data) return false;
    for (const key in data) {
      // Composite keys always have a colon followed by a number
      // Old keys are just URLs (e.g., "https://example.com/")
      const manifest = data[key];
      if (manifest.userContextId === undefined) {
        return true;
      }
    }
    return false;
  }

  /**
   * Migrates from old key format (start_url) to composite key format (start_url:userContextId).
   */
  private migrateToCompositeKeyFormat(
    data: Record<string, Manifest>,
  ): Record<string, Manifest> {
    const migrated: Record<string, Manifest> = {};
    for (const key in data) {
      const manifest = data[key];
      // Assign default userContextId if not present
      if (manifest.userContextId === undefined) {
        manifest.userContextId = 0;
      }
      const newKey = DataManager.buildKey(
        manifest.start_url,
        manifest.userContextId,
      );
      migrated[newKey] = manifest;
    }
    return migrated;
  }

  private extractIconSrc(entry: LegacyPWAEntry): string | undefined {
    const { manifest } = entry;

    if (!manifest.icons) return undefined;

    // Try to find an icon from the icons array
    if (Array.isArray(manifest.icons)) {
      // If icons is an array, check each item
      for (const icon of manifest.icons) {
        if (icon && typeof icon === "object" && "src" in icon) {
          return icon.src;
        }
      }
    } else if (manifest.icons.src && Array.isArray(manifest.icons.src)) {
      // Handle the structure defined in LegacyPWAEntry type
      if (manifest.icons.src.length > 0) {
        return manifest.icons.src[0].src;
      }
    }

    // If no icon found, return undefined
    return undefined;
  }

  private migrateFromLegacyFormat(
    legacyData: Record<string, LegacyPWAEntry>,
  ): Record<string, Manifest> {
    const migratedData: Record<string, Manifest> = {};

    for (const key in legacyData) {
      const entry = legacyData[key];
      const startUrl = entry.startURI || entry.manifest.start_url;
      const name = entry.name || "Unnamed App";

      // Create the basic manifest object with required fields
      const manifest: Manifest = {
        id: entry.id,
        name: name,
        short_name: entry.manifest.short_name || name,
        start_url: startUrl,
        scope: entry.scope || entry.manifest.scope || "",
        icon: "",
      };

      // Extract icon if available
      const iconSrc = this.extractIconSrc(entry);
      if (iconSrc) {
        manifest.icon = iconSrc;
      }

      // Copy any config values if they exist
      if (entry.config) {
        // @ts-expect-error - Adding non-standard property that may be in the data
        manifest.config = { ...entry.config };
      }

      migratedData[DataManager.buildKey(startUrl, 0)] = manifest;
    }

    return migratedData;
  }

  private async ensureStoreDirectory() {
    await IOUtils.makeDirectory(this.ssbStoreDirectory, {
      createAncestors: true,
      ignoreExisting: true,
    });
  }

  private async overrideCurrentSsbData(ssbData: object) {
    await this.ensureStoreDirectory();
    await IOUtils.writeJSON(this.ssbStoreFile, ssbData);
  }

  public async saveSsbData(manifest: Manifest) {
    const key = DataManager.buildKey(
      manifest.start_url,
      manifest.userContextId ?? 0,
    );
    const currentSsbData = await this.getCurrentSsbData();
    currentSsbData[key] = manifest;
    await this.overrideCurrentSsbData(currentSsbData);
  }

  public async removeSsbData(key: string) {
    const list = await this.getCurrentSsbData();
    if (list[key]) {
      delete list[key];
      await this.overrideCurrentSsbData(list);
    }
  }

  /**
   * Moves an SSB entry from one composite key to another in a single write.
   */
  public async moveSsbKey(
    oldKey: string,
    manifest: Manifest,
  ): Promise<boolean> {
    const newKey = DataManager.buildKey(
      manifest.start_url,
      manifest.userContextId ?? 0,
    );
    const list = await this.getCurrentSsbData();
    if (!list[oldKey]) {
      return false;
    }
    if (newKey !== oldKey && list[newKey]) {
      return false;
    }

    delete list[oldKey];
    list[newKey] = manifest;
    await this.overrideCurrentSsbData(list);
    return true;
  }
}
