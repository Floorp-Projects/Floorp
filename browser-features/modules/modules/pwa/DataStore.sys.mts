/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { buildSsbKey } from "#libs/pwa/ssbKeyUtils.ts";
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
  private schemaValidator:
    | Awaited<
      ReturnType<typeof getSchemaValidator>
    >
    | null = null;

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

    // Migrate old key format (start_url only) to composite key (start_url:userContextId)
    if (this.isOldKeyFormat(data as Record<string, Manifest>)) {
      const migratedData = this.migrateToCompositeKeyFormat(
        data as Record<string, Manifest>,
      );
      await this.overrideCurrentSsbData(migratedData);
      return migratedData;
    }

    // Validate data against schema (non-blocking warning only)
    await this.validateData(data);

    return data as Record<string, Manifest>;
  }

  private isLegacyFormat(data: Record<string, LegacyPWAEntry>): boolean {
    if (!data) return false;

    for (const key in data) {
      if (data[key].startURI && data[key].manifest) {
        return true;
      }
    }
    return false;
  }

  private isOldKeyFormat(data: Record<string, Manifest>): boolean {
    if (!data) return false;
    for (const key in data) {
      const manifest = data[key];
      if (manifest.userContextId === undefined) {
        return true;
      }
    }
    return false;
  }

  private migrateToCompositeKeyFormat(
    data: Record<string, Manifest>,
  ): Record<string, Manifest> {
    const migrated: Record<string, Manifest> = {};
    for (const key in data) {
      const manifest = data[key];
      if (manifest.userContextId === undefined) {
        manifest.userContextId = 0;
      }
      const newKey = buildSsbKey(manifest.start_url, manifest.userContextId);
      migrated[newKey] = manifest;
    }
    return migrated;
  }

  private extractIconSrc(entry: LegacyPWAEntry): string | undefined {
    const { manifest } = entry;

    if (!manifest.icons) return undefined;

    if (Array.isArray(manifest.icons)) {
      for (const icon of manifest.icons) {
        if (icon && typeof icon === "object" && "src" in icon) {
          return icon.src;
        }
      }
    } else if (manifest.icons.src && Array.isArray(manifest.icons.src)) {
      if (manifest.icons.src.length > 0) {
        return manifest.icons.src[0].src;
      }
    }

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

      const manifest: Manifest = {
        id: entry.id,
        name: name,
        short_name: entry.manifest.short_name || name,
        start_url: startUrl,
        scope: entry.scope || entry.manifest.scope || "",
        icon: "",
      };

      const iconSrc = this.extractIconSrc(entry);
      if (iconSrc) {
        manifest.icon = iconSrc;
      }

      if (entry.config) {
        // @ts-expect-error - Adding non-standard property that may be in the data
        manifest.config = { ...entry.config };
      }

      migratedData[buildSsbKey(startUrl, 0)] = manifest;
    }

    return migratedData;
  }

  private async overrideCurrentSsbData(ssbData: object) {
    await IOUtils.writeJSON(this.ssbStoreFile, ssbData);
  }

  public async saveSsbData(manifest: Manifest) {
    const key = buildSsbKey(manifest.start_url, manifest.userContextId ?? 0);
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
