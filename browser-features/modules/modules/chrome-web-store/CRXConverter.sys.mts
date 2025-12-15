/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * CRXConverter - Converts Chrome CRX files to Firefox XPI format
 *
 * This module orchestrates the conversion process:
 * - Parse CRX header and extract ZIP payload
 * - Validate source code for compatibility
 * - Transform manifest.json for Firefox compatibility
 * - Sanitize DNR rules
 * - Repack as XPI file
 *
 * Uses fflate for high-performance ZIP processing
 */

import type {
  ExtensionMetadata,
  ChromeManifest,
  ConversionResult,
  ConversionOptions,
} from "./types.ts";
import { CWSError, CWSErrorCode } from "./types.ts";
import { extractZipFromCRX } from "./CRXParser.sys.mts";
import {
  transformManifest,
  validateManifest,
} from "./ManifestTransformer.sys.mts";
import { sanitizeDNRRules } from "./DNRTransformer.sys.mts";
import {
  validateSourceCode,
  isJavaScriptFile,
  mightContainUnsupportedPatterns,
} from "./CodeValidator.sys.mts";
import { unzipSync, zipSync, type Unzipped } from "fflate/browser";
import { OFFSCREEN_POLYFILL_SOURCE } from "./polyfills/offscreen/OffscreenPolyfillSource.sys.mts";
import { DOCUMENT_ID_POLYFILL_SOURCE } from "./polyfills/documentId/DocumentIdPolyfillSource.sys.mts";

// =============================================================================
// Constants
// =============================================================================

/** Number of files to process before yielding to event loop */
const YIELD_INTERVAL = 50;

/** Default conversion options */
const DEFAULT_OPTIONS: Required<ConversionOptions> = {
  validateCode: true,
  sanitizeDNR: true,
  minGeckoVersion: "115.0",
} as const;

// =============================================================================
// Types
// =============================================================================

interface ProcessZipResult {
  modifiedZip: ArrayBuffer | null;
  originalManifest: ChromeManifest | null;
  warnings: string[];
}

type LogFunction = (step: string) => void;

// =============================================================================
// Utilities
// =============================================================================

/**
 * Yield to the event loop to prevent UI freezing during heavy operations
 * Uses main thread dispatch for proper browser integration
 */
function yieldToEventLoop(): Promise<void> {
  return new Promise((resolve) => {
    Services.tm.dispatchToMainThread(() => resolve());
  });
}

/**
 * Create a logging function with timing information
 * Disabled to reduce console output
 */
function createLogger(): LogFunction {
  return () => {
    // Logging disabled
  };
}

// =============================================================================
// CRXConverter Class
// =============================================================================

/**
 * Converter for Chrome CRX to Firefox XPI format
 */
export class CRXConverterClass {
  private readonly options: Required<ConversionOptions>;

  constructor(options: ConversionOptions = {}) {
    this.options = { ...DEFAULT_OPTIONS, ...options };
  }

  /**
   * Convert a CRX file to XPI format
   * @param crxData - Raw CRX file data
   * @param extensionId - Chrome extension ID
   * @param metadata - Extension metadata
   * @returns Conversion result with XPI data or error
   */
  async convert(
    crxData: ArrayBuffer,
    extensionId: string,
    metadata: ExtensionMetadata,
  ): Promise<ConversionResult> {
    const warnings: string[] = [];
    const log = createLogger();

    try {
      log(`Starting conversion for ${extensionId} (${metadata.name})`);

      // Yield before heavy operation
      await yieldToEventLoop();

      // Step 1: Parse CRX header and extract ZIP
      log("Extracting ZIP from CRX...");
      const zipData = extractZipFromCRX(crxData);
      if (!zipData) {
        throw new CWSError(
          CWSErrorCode.INVALID_CRX,
          "Failed to extract ZIP from CRX file",
        );
      }

      log(`Extracted ZIP: ${zipData.byteLength} bytes`);

      // Yield before heavy ZIP processing
      await yieldToEventLoop();

      // Step 2: Parse and transform the manifest using fflate
      log("Processing ZIP contents with fflate...");
      const result = await this.processZipContentsWithFflate(
        zipData,
        extensionId,
        log,
      );
      log("ZIP processing complete");

      if (!result.modifiedZip) {
        // Return original ZIP if modification failed but no error thrown
        log("Could not modify manifest, using original ZIP");
        return {
          success: true,
          xpiData: zipData,
          warnings: [...warnings, ...result.warnings],
        };
      }

      warnings.push(...result.warnings);

      log(
        `Conversion complete. Manifest version: ${result.originalManifest?.manifest_version}`,
      );

      return {
        success: true,
        xpiData: result.modifiedZip,
        originalManifest: result.originalManifest ?? undefined,
        warnings,
      };
    } catch (error) {
      log(`Conversion failed: ${error}`);

      if (error instanceof CWSError) {
        return { success: false, error, warnings };
      }

      return {
        success: false,
        error: new CWSError(
          CWSErrorCode.CONVERSION_FAILED,
          error instanceof Error ? error.message : String(error),
          error,
        ),
        warnings,
      };
    }
  }

  /**
   * Process ZIP contents using fflate for high performance
   */
  private async processZipContentsWithFflate(
    zipData: ArrayBuffer,
    extensionId: string,
    log: LogFunction,
  ): Promise<ProcessZipResult> {
    const warnings: string[] = [];

    // Unzip using fflate
    log("Unzipping with fflate...");
    const zipBytes = new Uint8Array(zipData);
    const unzipped = unzipSync(zipBytes);
    const entryCount = Object.keys(unzipped).length;
    log(`Unzipped ${entryCount} entries`);

    // Yield after unzip
    await yieldToEventLoop();

    // Read manifest.json
    const manifestBytes = unzipped["manifest.json"];
    if (!manifestBytes) {
      throw new CWSError(
        CWSErrorCode.INVALID_MANIFEST,
        "No manifest.json found in extension",
      );
    }

    const manifestContent = new TextDecoder().decode(manifestBytes);

    // Parse and validate manifest
    log("Parsing manifest...");
    const parsedManifest = this.parseManifest(manifestContent);
    if (!parsedManifest) {
      throw new CWSError(
        CWSErrorCode.INVALID_MANIFEST,
        "Invalid manifest.json structure",
      );
    }

    // Transform manifest for Firefox
    log("Transforming manifest for Firefox...");
    const firefoxManifest = transformManifest(parsedManifest, extensionId);
    log("Manifest transformed");

    // Identify DNR rule files
    const dnrRuleFiles = this.identifyDNRRuleFiles(parsedManifest);

    // Yield before processing files
    await yieldToEventLoop();

    // Process and create new ZIP
    log("Creating output XPI...");
    const outputFiles: Unzipped = {};

    // Add transformed manifest
    const manifestJson = JSON.stringify(firefoxManifest, null, 2);
    outputFiles["manifest.json"] = new TextEncoder().encode(manifestJson);

    // Inject Offscreen Polyfill if needed
    if (
      firefoxManifest.background?.scripts?.includes(
        "__floorp_polyfills__/OffscreenPolyfill.js",
      )
    ) {
      log("Injecting Offscreen Polyfill source...");
      outputFiles["__floorp_polyfills__/OffscreenPolyfill.js"] =
        new TextEncoder().encode(OFFSCREEN_POLYFILL_SOURCE);
    }

    // Inject DocumentId Polyfill if needed
    if (
      firefoxManifest.background?.scripts?.includes(
        "__floorp_polyfills__/DocumentIdPolyfill.js",
      )
    ) {
      log("Injecting DocumentId Polyfill source...");
      outputFiles["__floorp_polyfills__/DocumentIdPolyfill.js"] =
        new TextEncoder().encode(DOCUMENT_ID_POLYFILL_SOURCE);
    }

    // Process other files
    let processedCount = 0;
    for (const [filename, data] of Object.entries(unzipped)) {
      if (filename === "manifest.json") {
        continue; // Already added transformed version
      }

      // Validate JavaScript files if enabled
      if (this.options.validateCode && isJavaScriptFile(filename)) {
        const content = new TextDecoder().decode(data);
        if (mightContainUnsupportedPatterns(content)) {
          const error = validateSourceCode(filename, content);
          if (error) {
            throw new CWSError(
              CWSErrorCode.UNSUPPORTED_CODE,
              `${error} in ${filename}`,
            );
          }
        }
      }

      // Sanitize DNR rules if needed
      if (this.options.sanitizeDNR && dnrRuleFiles.has(filename)) {
        try {
          const jsonString = new TextDecoder().decode(data);
          const rules = JSON.parse(jsonString) as unknown[];
          const sanitizedRules = sanitizeDNRRules(rules);
          const sanitizedJson = JSON.stringify(sanitizedRules, null, 2);
          outputFiles[filename] = new TextEncoder().encode(sanitizedJson);
        } catch (e) {
          // Fallback to original with warning
          warnings.push(
            `Failed to sanitize DNR rules in ${filename}: ${
              e instanceof Error ? e.message : String(e)
            }`,
          );
          outputFiles[filename] = data;
        }
      } else {
        // Copy as-is
        outputFiles[filename] = data;
      }

      processedCount++;

      // Yield periodically to prevent UI freezing
      if (processedCount % YIELD_INTERVAL === 0) {
        await yieldToEventLoop();
      }
    }

    log(`Processed ${processedCount} entries`);

    // Yield before final zip
    await yieldToEventLoop();

    // Create output ZIP using fflate (no compression for speed)
    log("Zipping output XPI with fflate...");
    const outputZip = zipSync(outputFiles, { level: 0 }); // No compression
    log(`Output XPI size: ${outputZip.byteLength} bytes`);

    return {
      modifiedZip: outputZip.buffer as ArrayBuffer,
      originalManifest: parsedManifest,
      warnings,
    };
  }

  /**
   * Identify DNR rule files from manifest
   */
  private identifyDNRRuleFiles(manifest: ChromeManifest): Set<string> {
    const dnrRuleFiles = new Set<string>();
    const dnr = manifest.declarative_net_request;

    if (dnr?.rule_resources) {
      for (const resource of dnr.rule_resources) {
        if (resource.path) {
          const normalizedPath = resource.path.replace(/^\.\//, "");
          dnrRuleFiles.add(normalizedPath);
          dnrRuleFiles.add(resource.path);
        }
      }
    }

    return dnrRuleFiles;
  }

  /**
   * Parse and validate manifest content
   */
  private parseManifest(content: string): ChromeManifest | null {
    try {
      const parsed: unknown = JSON.parse(content);
      if (!validateManifest(parsed)) {
        return null;
      }
      return parsed;
    } catch {
      return null;
    }
  }
}

// =============================================================================
// Legacy API (for backwards compatibility)
// =============================================================================

/**
 * Legacy converter object for backwards compatibility
 */
export const CRXConverter = {
  /**
   * Convert a CRX file to XPI format (async)
   * @deprecated Use CRXConverterClass instead
   */
  async convert(
    crxData: ArrayBuffer,
    extensionId: string,
    metadata: ExtensionMetadata,
  ): Promise<ArrayBuffer | null> {
    const converter = new CRXConverterClass();
    const result = await converter.convert(crxData, extensionId, metadata);

    if (!result.success) {
      if (result.error) {
        throw result.error;
      }
      return null;
    }

    return result.xpiData ?? null;
  },
};
