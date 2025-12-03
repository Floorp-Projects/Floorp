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
import {
  writeArrayBufferToFile,
  readFileToArrayBuffer,
  readInputStreamToBuffer,
  createInputStream,
  createUniqueTempFile,
  safeRemoveFile,
} from "./FileUtils.sys.mts";

// =============================================================================
// Types
// =============================================================================

interface FileEntry {
  name: string;
  data: ArrayBuffer;
}

interface ModifyManifestResult {
  modifiedZip: ArrayBuffer | null;
  originalManifest: ChromeManifest | null;
  warnings: string[];
}

// =============================================================================
// Default Options
// =============================================================================

const DEFAULT_OPTIONS: Required<ConversionOptions> = {
  validateCode: true,
  sanitizeDNR: true,
  minGeckoVersion: "115.0",
};

// =============================================================================
// CRXConverter Class
// =============================================================================

/**
 * Converter for Chrome CRX to Firefox XPI format
 */
export class CRXConverterClass {
  private options: Required<ConversionOptions>;

  constructor(options: ConversionOptions = {}) {
    this.options = { ...DEFAULT_OPTIONS, ...options };
  }

  /**
   * Convert a CRX file to XPI format
   * @param crxData - Raw CRX file data
   * @param extensionId - Chrome extension ID
   * @param metadata - Extension metadata
   * @returns Conversion result
   */
  convert(
    crxData: ArrayBuffer,
    extensionId: string,
    metadata: ExtensionMetadata,
  ): ConversionResult {
    const warnings: string[] = [];

    try {
      console.log(
        `[CRXConverter] Starting conversion for ${extensionId} (${metadata.name})`,
      );

      // Step 1: Parse CRX header and extract ZIP
      const zipData = extractZipFromCRX(crxData);
      if (!zipData) {
        throw new CWSError(
          CWSErrorCode.INVALID_CRX,
          "Failed to extract ZIP from CRX file",
        );
      }

      console.log(`[CRXConverter] Extracted ZIP: ${zipData.byteLength} bytes`);

      // Step 2: Parse and transform the manifest
      const result = this.processZipContents(zipData, extensionId);

      if (!result.modifiedZip) {
        // Return original ZIP if modification failed but no error thrown
        console.warn(
          "[CRXConverter] Could not modify manifest, using original ZIP",
        );
        return {
          success: true,
          xpiData: zipData,
          warnings: [...warnings, ...result.warnings],
        };
      }

      warnings.push(...result.warnings);

      console.log(
        "[CRXConverter] Conversion complete. Manifest version:",
        result.originalManifest?.manifest_version,
      );

      return {
        success: true,
        xpiData: result.modifiedZip,
        originalManifest: result.originalManifest ?? undefined,
        warnings,
      };
    } catch (error) {
      console.error("[CRXConverter] Conversion failed:", error);

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
   * Process ZIP contents: extract, validate, transform, and repack
   */
  private processZipContents(
    zipData: ArrayBuffer,
    extensionId: string,
  ): ModifyManifestResult {
    const warnings: string[] = [];
    let inputFile: nsIFile | null = null;
    let outputFile: nsIFile | null = null;

    try {
      // Create temporary file for input ZIP
      inputFile = createUniqueTempFile(`crx-input-${extensionId}`, "zip");
      writeArrayBufferToFile(inputFile, zipData);

      // Open and read the ZIP
      const zipReader = this.openZipReader(inputFile);
      const { fileEntries, manifestContent, dnrRuleFiles } =
        this.extractZipEntries(zipReader, warnings);
      zipReader.close();

      // Clean up input file
      safeRemoveFile(inputFile);
      inputFile = null;

      if (!manifestContent) {
        throw new CWSError(
          CWSErrorCode.INVALID_MANIFEST,
          "No manifest.json found in extension",
        );
      }

      // Parse and validate manifest
      const parsedManifest = this.parseManifest(manifestContent);
      if (!parsedManifest) {
        throw new CWSError(
          CWSErrorCode.INVALID_MANIFEST,
          "Invalid manifest.json structure",
        );
      }

      // Transform manifest for Firefox
      const firefoxManifest = transformManifest(parsedManifest, extensionId);

      // Create output XPI
      outputFile = createUniqueTempFile(`xpi-output-${extensionId}`, "xpi");
      this.writeXpiFile(outputFile, fileEntries, firefoxManifest, dnrRuleFiles);

      // Read the output XPI
      const modifiedZip = readFileToArrayBuffer(outputFile);

      // Clean up output file
      safeRemoveFile(outputFile);
      outputFile = null;

      return {
        modifiedZip,
        originalManifest: parsedManifest,
        warnings,
      };
    } finally {
      // Ensure cleanup
      if (inputFile) safeRemoveFile(inputFile);
      if (outputFile) safeRemoveFile(outputFile);
    }
  }

  /**
   * Open a ZIP file for reading
   */
  private openZipReader(file: nsIFile): nsIZipReader {
    const ZipReader = Components.Constructor(
      "@mozilla.org/libjar/zip-reader;1",
      "nsIZipReader",
      "open",
    );
    return new ZipReader(file) as nsIZipReader;
  }

  /**
   * Extract all entries from a ZIP file
   */
  private extractZipEntries(
    zipReader: nsIZipReader,
    _warnings: string[],
  ): {
    fileEntries: FileEntry[];
    manifestContent: string | null;
    dnrRuleFiles: Set<string>;
  } {
    const fileEntries: FileEntry[] = [];
    let manifestContent: string | null = null;
    const dnrRuleFiles = new Set<string>();
    const entries = zipReader.findEntries("*");

    while (entries.hasMore()) {
      const entryName = entries.getNext();
      const entry = zipReader.getEntry(entryName);

      // Skip directories
      if (!entry || entry.isDirectory || entryName.endsWith("/")) {
        continue;
      }

      const inputStream = zipReader.getInputStream(entryName);
      const buffer = readInputStreamToBuffer(inputStream);

      if (entryName === "manifest.json") {
        manifestContent = new TextDecoder().decode(new Uint8Array(buffer));

        // Check for DNR rule files
        try {
          const manifest = JSON.parse(manifestContent);
          const dnr = manifest.declarative_net_request;
          if (dnr?.rule_resources) {
            for (const resource of dnr.rule_resources) {
              if (resource.path) {
                // Normalize path (remove leading ./)
                const normalizedPath = resource.path.replace(/^\.\//, "");
                dnrRuleFiles.add(normalizedPath);
                dnrRuleFiles.add(resource.path);
              }
            }
          }
        } catch {
          // Ignore parse errors here, will be caught later
        }
      } else if (this.options.validateCode && isJavaScriptFile(entryName)) {
        // Validate JavaScript files
        const content = new TextDecoder().decode(new Uint8Array(buffer));

        // Quick check before full validation
        if (mightContainUnsupportedPatterns(content)) {
          const error = validateSourceCode(entryName, content);
          if (error) {
            console.error(
              `[CRXConverter] Validation failed for ${entryName}: ${error}`,
            );
            throw new CWSError(
              CWSErrorCode.UNSUPPORTED_CODE,
              `${error} in ${entryName}`,
            );
          }
        }
      }

      fileEntries.push({ name: entryName, data: buffer });
    }

    return { fileEntries, manifestContent, dnrRuleFiles };
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
    } catch (error) {
      console.error("[CRXConverter] Failed to parse manifest:", error);
      return null;
    }
  }

  /**
   * Write the XPI file with transformed contents
   */
  private writeXpiFile(
    outputFile: nsIFile,
    fileEntries: FileEntry[],
    manifest: ChromeManifest,
    dnrRuleFiles: Set<string>,
  ): void {
    const zipWriter = Cc["@mozilla.org/zipwriter;1"].createInstance(
      Ci.nsIZipWriter,
    );

    // PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE
    zipWriter.open(outputFile, 0x02 | 0x08 | 0x20);

    try {
      // Write transformed manifest first
      const manifestBytes = new TextEncoder().encode(
        JSON.stringify(manifest, null, 2),
      );
      const manifestStream = createInputStream(manifestBytes);
      zipWriter.addEntryStream(
        "manifest.json",
        Date.now() * 1000,
        Ci.nsIZipWriter.COMPRESSION_DEFAULT as number,
        manifestStream,
        false,
      );

      // Write other files
      for (const entry of fileEntries) {
        if (entry.name === "manifest.json") {
          continue; // Already written
        }

        if (this.options.sanitizeDNR && dnrRuleFiles.has(entry.name)) {
          this.writeSanitizedDNREntry(zipWriter, entry);
        } else {
          this.writeEntry(zipWriter, entry);
        }
      }
    } finally {
      zipWriter.close();
    }
  }

  /**
   * Write a sanitized DNR rules entry
   */
  private writeSanitizedDNREntry(
    zipWriter: nsIZipWriter,
    entry: FileEntry,
  ): void {
    try {
      const jsonString = new TextDecoder().decode(new Uint8Array(entry.data));
      const rules = JSON.parse(jsonString);
      const sanitizedRules = sanitizeDNRRules(rules);
      const sanitizedBytes = new TextEncoder().encode(
        JSON.stringify(sanitizedRules, null, 2),
      );
      const stream = createInputStream(sanitizedBytes);

      zipWriter.addEntryStream(
        entry.name,
        Date.now() * 1000,
        Ci.nsIZipWriter.COMPRESSION_DEFAULT as number,
        stream,
        false,
      );

      console.log(`[CRXConverter] Sanitized DNR rules: ${entry.name}`);
    } catch (error) {
      console.error(
        `[CRXConverter] Failed to sanitize DNR rules for ${entry.name}:`,
        error,
      );
      // Fallback to original file
      this.writeEntry(zipWriter, entry);
    }
  }

  /**
   * Write a file entry as-is
   */
  private writeEntry(zipWriter: nsIZipWriter, entry: FileEntry): void {
    const stream = createInputStream(new Uint8Array(entry.data));
    zipWriter.addEntryStream(
      entry.name,
      Date.now() * 1000,
      Ci.nsIZipWriter.COMPRESSION_DEFAULT as number,
      stream,
      false,
    );
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
   * Convert a CRX file to XPI format
   * @deprecated Use CRXConverterClass instead
   */
  convert(
    crxData: ArrayBuffer,
    extensionId: string,
    metadata: ExtensionMetadata,
  ): ArrayBuffer | null {
    const converter = new CRXConverterClass();
    const result = converter.convert(crxData, extensionId, metadata);

    if (!result.success) {
      if (result.error) {
        throw result.error;
      }
      return null;
    }

    return result.xpiData ?? null;
  },
};
