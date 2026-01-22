/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type {
  LegacyPWAEntry,
  Manifest,
} from "#features-chrome/common/pwa/type.ts";

const { JsonSchema } = ChromeUtils.importESModule(
  "resource://gre/modules/JsonSchema.sys.mjs",
);

// Schema URL for validation
const SSB_SCHEMA_URL = "resource://noraneko/modules/pwa/ssb.schema.json";

/**
 * Get JSON Schema validator for SSB data
 */
async function getSchemaValidator() {
  const response = await fetch(SSB_SCHEMA_URL);
  const schema = await response.json();
  return new JsonSchema!.Validator(schema);
}

export class DataManager {
  private schemaValidator: Awaited<
    ReturnType<typeof getSchemaValidator>
  > | null = null;

  private get ssbStoreFile() {
    return PathUtils.join(PathUtils.profileDir, "ssb", "ssb.json");
  }

  constructor() {}

  /**
   * Initialize schema validator lazily
   */
  private async getValidator() {
    if (!this.schemaValidator) {
      try {
        this.schemaValidator = await getSchemaValidator();
      } catch (e) {
        console.warn("[DataManager] Failed to load schema validator:", e);
      }
    }
    return this.schemaValidator;
  }

  /**
   * Validate data against JSON schema
   */
  private async validateData(data: unknown): Promise<boolean> {
    const validator = await this.getValidator();
    if (!validator) {
      // If validator failed to load, skip validation
      return true;
    }

    const result = validator.validate(data);
    if (!result.valid) {
      console.warn("[DataManager] Schema validation failed:", result.errors);
      return false;
    }
    return true;
  }

  public async getCurrentSsbData(): Promise<Record<string, Manifest>> {
    const fileExists = await IOUtils.exists(this.ssbStoreFile);
    if (!fileExists) {
      await IOUtils.writeJSON(this.ssbStoreFile, {});
      return {};
    }

    const data = await IOUtils.readJSON(this.ssbStoreFile);

    // Check if this is legacy format and migrate if needed
    if (this.isLegacyFormat(data)) {
      console.log("Migrating from legacy PWA data format");
      const migratedData = this.migrateFromLegacyFormat(
        data as Record<string, LegacyPWAEntry>,
      );
      await this.overrideCurrentSsbData(migratedData);
      return migratedData;
    }

    // Validate data against schema (non-blocking warning only)
    await this.validateData(data);

    return data as Record<string, Manifest>;
  }

  private isLegacyFormat(data: Record<string, LegacyPWAEntry>): boolean {
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
        // @ts-ignore - Adding non-standard property that may be in the data
        manifest.config = { ...entry.config };
      }

      migratedData[startUrl] = manifest;
    }

    return migratedData;
  }

  private async overrideCurrentSsbData(ssbData: object) {
    await IOUtils.writeJSON(this.ssbStoreFile, ssbData);
  }

  public async saveSsbData(manifest: Manifest) {
    const start_url = manifest.start_url;
    const currentSsbData = await this.getCurrentSsbData();
    currentSsbData[start_url] = manifest;
    await this.overrideCurrentSsbData(currentSsbData);
  }

  public async removeSsbData(url: string) {
    const list = await this.getCurrentSsbData();
    if (list[url]) {
      delete list[url];
      await this.overrideCurrentSsbData(list);
    }
  }
}

export class DataStoreProvider {
  private static instance: DataManager | null = null;

  static getDataManager(): DataManager {
    if (!this.instance) {
      this.instance = new DataManager();
    }
    return this.instance;
  }
}
