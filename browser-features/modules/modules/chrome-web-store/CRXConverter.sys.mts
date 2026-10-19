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
  ChromeConversionReport,
  ChromeManifest,
  ConversionOptions,
  ConversionResult,
  ExtensionMetadata,
} from "./types.ts";
import { CWSError, CWSErrorCode } from "./types.ts";
import { type VerifiedCRX, verifyCRX } from "./CRXVerifier.sys.mts";
import {
  checkChromeVersionRequirement,
  transformManifestFull,
  validateManifest,
} from "./ManifestTransformer.sys.mts";
import { sanitizeDNRRulesFull } from "./DNRTransformer.sys.mts";
import {
  isJavaScriptFile,
  mightContainUnsupportedPatterns,
  validateSourceCodeFull,
} from "./CodeValidator.sys.mts";
import { type Unzipped, unzipSync, zipSync } from "fflate/browser";
import {
  GECKO_MIN_VERSION,
  generateFirefoxId,
  isChromeExtensionId,
  MAX_CRX_ENTRIES,
  MAX_EXPANDED_CRX_SIZE,
  MAX_FILE_SIZE,
} from "./Constants.sys.mts";

// =============================================================================
// Constants
// =============================================================================

/** Number of files to process before yielding to event loop */
const YIELD_INTERVAL = 50;

export const ORIGINAL_MANIFEST_PATH =
  "__floorp_chrome__/original-manifest.json";
export const CONVERSION_REPORT_PATH =
  "__floorp_chrome__/conversion-report.json";

/** Default conversion options */
const DEFAULT_OPTIONS: Required<ConversionOptions> = {
  validateCode: true,
  sanitizeDNR: true,
  minGeckoVersion: GECKO_MIN_VERSION,
} as const;

// =============================================================================
// Types
// =============================================================================

interface ProcessZipResult {
  modifiedZip: ArrayBuffer;
  originalManifest: ChromeManifest;
  report: ChromeConversionReport;
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

      if (!isChromeExtensionId(extensionId)) {
        throw new CWSError(
          CWSErrorCode.VALIDATION_ERROR,
          "Invalid Chrome extension ID",
        );
      }

      // Yield before heavy operation
      await yieldToEventLoop();

      // Step 1: Parse CRX header and extract ZIP
      log("Extracting ZIP from CRX...");
      let verified: VerifiedCRX;
      try {
        verified = await verifyCRX(crxData, extensionId);
      } catch (cause) {
        throw new CWSError(
          CWSErrorCode.INVALID_CRX,
          cause instanceof Error
            ? cause.message
            : "CRX signature verification failed",
          cause,
        );
      }
      const zipData = verified.zipData;

      log(`Extracted ZIP: ${zipData.byteLength} bytes`);

      // Yield before heavy ZIP processing
      await yieldToEventLoop();

      // Step 2: Parse and transform the manifest using fflate
      log("Processing ZIP contents with fflate...");
      const result = await this.processZipContentsWithFflate(
        zipData,
        extensionId,
        log,
        {
          format: verified.version,
          algorithm: verified.algorithm,
          publicKeySha256: verified.publicKeySha256,
        },
      );
      log("ZIP processing complete");

      warnings.push(...result.warnings);

      log(
        `Conversion complete. Manifest version: ${result.originalManifest?.manifest_version}`,
      );

      return {
        success: true,
        xpiData: result.modifiedZip,
        originalManifest: result.originalManifest ?? undefined,
        report: result.report,
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
    signature: NonNullable<ChromeConversionReport["signature"]>,
  ): Promise<ProcessZipResult> {
    const warnings: string[] = [];

    // Unzip using fflate
    log("Unzipping with fflate...");
    const zipBytes = new Uint8Array(zipData);
    let expandedSize = 0;
    let fileCount = 0;
    const unzipped = unzipSync(zipBytes, {
      filter(file) {
        expandedSize += file.originalSize;
        fileCount++;
        if (
          file.originalSize > MAX_FILE_SIZE ||
          expandedSize > MAX_EXPANDED_CRX_SIZE || fileCount > MAX_CRX_ENTRIES
        ) {
          throw new CWSError(
            CWSErrorCode.VALIDATION_ERROR,
            "Chrome extension archive exceeds the expansion limits",
          );
        }
        return true;
      },
    });
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
    const versionRequirement = checkChromeVersionRequirement(parsedManifest);
    if (versionRequirement) {
      throw new CWSError(CWSErrorCode.VALIDATION_ERROR, versionRequirement);
    }

    if (!isChromeExtensionId(extensionId)) {
      throw new CWSError(
        CWSErrorCode.VALIDATION_ERROR,
        "Invalid Chrome extension ID",
      );
    }

    if (
      Object.keys(unzipped).some((name) => {
        const normalized = name.replace(/^(\.\/)+/, "");
        return normalized === "__floorp_chrome__" ||
          normalized.startsWith("__floorp_chrome__/");
      })
    ) {
      throw new CWSError(
        CWSErrorCode.VALIDATION_ERROR,
        "Extension uses the reserved __floorp_chrome__ package directory",
      );
    }

    // Transform manifest for Firefox
    log("Transforming manifest for Firefox...");
    const transformed = transformManifestFull(parsedManifest, {
      extensionId,
      minGeckoVersion: this.options.minGeckoVersion,
    });
    const firefoxManifest = transformed.manifest;
    warnings.push(...transformed.warnings);
    for (const permission of transformed.removedPermissions) {
      warnings.push(`Unsupported permission removed: ${permission}`);
    }
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
    outputFiles["manifest.json"] = new Uint8Array(
      new TextEncoder().encode(manifestJson),
    );
    outputFiles[ORIGINAL_MANIFEST_PATH] = manifestBytes;

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
          const validation = validateSourceCodeFull(filename, content);
          if (!validation.valid) {
            throw new CWSError(
              CWSErrorCode.UNSUPPORTED_CODE,
              `${validation.errors[0].message} in ${filename}`,
            );
          }
          warnings.push(
            ...validation.warnings.map(
              (warning) => `${filename}: ${warning.message}`,
            ),
          );
        }
      }

      // Sanitize DNR rules if needed
      if (this.options.sanitizeDNR && dnrRuleFiles.has(filename)) {
        try {
          const jsonString = new TextDecoder().decode(data);
          const rules: unknown = JSON.parse(jsonString);
          const sanitized = sanitizeDNRRulesFull(rules);
          if (sanitized.rejectedRules > 0) {
            throw new TypeError(sanitized.warnings.join("; "));
          }
          warnings.push(
            ...sanitized.warnings.map((warning) => `${filename}: ${warning}`),
          );
          const sanitizedJson = JSON.stringify(sanitized.rules, null, 2);
          outputFiles[filename] = new Uint8Array(
            new TextEncoder().encode(sanitizedJson),
          );
        } catch (e) {
          throw new CWSError(
            CWSErrorCode.VALIDATION_ERROR,
            `Incompatible DNR rules in ${filename}: ${
              e instanceof Error ? e.message : String(e)
            }`,
            e,
          );
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

    const report: ChromeConversionReport = {
      schemaVersion: 1,
      chromeId: extensionId,
      addonId: generateFirefoxId(extensionId),
      originalVersion: parsedManifest.version,
      originalManifestPath: ORIGINAL_MANIFEST_PATH,
      signature,
      warnings: [...warnings],
      removedPermissions: [...transformed.removedPermissions],
    };
    outputFiles[CONVERSION_REPORT_PATH] = new Uint8Array(
      new TextEncoder().encode(
        JSON.stringify(report, null, 2),
      ),
    );

    // Yield before final zip
    await yieldToEventLoop();

    // Create output ZIP using fflate (no compression for speed)
    log("Zipping output XPI with fflate...");
    const outputZip = zipSync(outputFiles, { level: 0 }); // No compression
    log(`Output XPI size: ${outputZip.byteLength} bytes`);

    return {
      modifiedZip: outputZip.buffer as ArrayBuffer,
      originalManifest: parsedManifest,
      report,
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
