/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * CRXConverter - Converts Chrome CRX files to Firefox XPI format
 *
 * This module orchestrates the conversion process:
 * - Parse CRX header and extract ZIP payload
 * - Transform manifest.json for Firefox compatibility
 * - Repack as XPI file
 *
 * Implementation details are delegated to specialized modules:
 * - crxParser.ts: CRX file format parsing
 * - manifestTransformer.ts: Manifest transformation
 * - fileUtils.ts: File I/O operations
 */

import type { ExtensionMetadata, ChromeManifest } from "./types.ts";
import { extractZipFromCRX } from "./CRXParser.sys.mts";
import {
  transformManifest,
  validateManifest,
} from "./ManifestTransformer.sys.mts";
import { sanitizeDNRRules } from "./DNRTransformer.sys.mts";
import { validateSourceCode } from "./CodeValidator.sys.mts";
import {
  writeArrayBufferToFile,
  readFileToArrayBuffer,
  readInputStreamToBuffer,
  createInputStream,
} from "./FileUtils.sys.mts";

export const CRXConverter = {
  /**
   * Convert a CRX file to XPI format
   */
  convert(
    crxData: ArrayBuffer,
    extensionId: string,
    _metadata: ExtensionMetadata,
  ): ArrayBuffer | null {
    try {
      console.log("[CRXConverter] Starting conversion...");

      // Step 1: Parse CRX header and extract ZIP
      const zipData = extractZipFromCRX(crxData);
      if (!zipData) {
        throw new Error("Failed to extract ZIP from CRX");
      }

      // Step 2: Parse and transform the manifest
      const { modifiedZip, originalManifest } = this.modifyManifest(
        zipData,
        extensionId,
      );

      if (!modifiedZip) {
        console.warn(
          "[CRXConverter] Could not modify manifest, using original ZIP",
        );
        return zipData;
      }

      console.log(
        "[CRXConverter] Conversion complete. Original manifest version:",
        originalManifest?.manifest_version,
      );

      return modifiedZip;
    } catch (error) {
      console.error("[CRXConverter] Conversion failed:", error);
      // Re-throw the error so the caller can handle it
      throw error;
    }
  },

  /**
   * Modify the manifest.json in the ZIP for Firefox compatibility
   */
  modifyManifest(
    zipData: ArrayBuffer,
    extensionId: string,
  ): {
    modifiedZip: ArrayBuffer | null;
    originalManifest: ChromeManifest | null;
  } {
    try {
      // Use the ZipReader API available in Firefox
      const ZipReader = Components.Constructor(
        "@mozilla.org/libjar/zip-reader;1",
        "nsIZipReader",
        "open",
      );

      // Create a temporary file for the input ZIP
      const tempDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
      const inputFile = tempDir.clone();
      inputFile.append(`crx-input-${extensionId}.zip`);

      // Write ZIP data to temp file
      writeArrayBufferToFile(inputFile, zipData);

      // Read the ZIP
      const zipReader = new ZipReader(inputFile);
      const entries = zipReader.findEntries("*");

      let manifestContent: string | null = null;
      const fileEntries: Array<{ name: string; data: ArrayBuffer }> = [];

      // Extract all entries
      while (entries.hasMore()) {
        const entryName = entries.getNext();
        const entry = zipReader.getEntry(entryName);

        if (!entry || entry.isDirectory) continue;

        const inputStream = zipReader.getInputStream(entryName);
        const buffer = readInputStreamToBuffer(inputStream);

        if (entryName === "manifest.json") {
          manifestContent = new TextDecoder().decode(new Uint8Array(buffer));
        } else if (entryName.endsWith(".js")) {
          // Validate JS content for unsupported patterns
          console.log(
            `[CRXConverter] Validating JS file: ${entryName} (${buffer.byteLength} bytes)`,
          );
          const content = new TextDecoder().decode(new Uint8Array(buffer));
          const error = validateSourceCode(entryName, content);
          if (error) {
            console.error(
              `[CRXConverter] Validation failed for ${entryName}: ${error}`,
            );
            throw new Error(
              `Extension contains unsupported code: ${error} in ${entryName}`,
            );
          }
        }

        fileEntries.push({
          name: entryName,
          data: buffer,
        });
      }

      zipReader.close();

      // Clean up input temp file
      try {
        inputFile.remove(false);
      } catch {
        // Ignore cleanup errors
      }

      if (!manifestContent) {
        console.error("[CRXConverter] No manifest.json found in CRX");
        return { modifiedZip: null, originalManifest: null };
      }

      // Parse manifest
      const parsedManifest: unknown = JSON.parse(manifestContent);

      if (!validateManifest(parsedManifest)) {
        console.error("[CRXConverter] Invalid manifest.json structure");
        return { modifiedZip: null, originalManifest: null };
      }

      const originalManifest = parsedManifest;

      // Identify DNR rule files to sanitize
      const dnrRuleFiles = new Set<string>();
      const dnr = originalManifest.declarative_net_request as
        | { rule_resources?: Array<{ path: string }> }
        | undefined;

      if (dnr?.rule_resources && Array.isArray(dnr.rule_resources)) {
        for (const resource of dnr.rule_resources) {
          if (resource.path) {
            dnrRuleFiles.add(resource.path);
          }
        }
      }

      // Transform manifest for Firefox
      const firefoxManifest = transformManifest(originalManifest, extensionId);

      // Create output ZIP with modified manifest
      const outputFile = tempDir.clone();
      outputFile.append(`xpi-output-${extensionId}.xpi`);

      // Make sure output file doesn't exist
      if (outputFile.exists()) {
        outputFile.remove(false);
      }

      const zipWriter = Cc["@mozilla.org/zipwriter;1"].createInstance(
        Ci.nsIZipWriter,
      );
      zipWriter.open(outputFile, 0x02 | 0x08 | 0x20); // PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE

      for (const entry of fileEntries) {
        if (entry.name === "manifest.json") {
          // Write transformed manifest
          const manifestBytes = new TextEncoder().encode(
            JSON.stringify(firefoxManifest, null, 2),
          );
          const manifestStream = createInputStream(manifestBytes);
          // COMPRESSION_DEFLATE = 9
          zipWriter.addEntryStream(
            entry.name,
            Date.now() * 1000,
            Ci.nsIZipWriter.COMPRESSION_DEFAULT as number,
            manifestStream,
            false,
          );
        } else if (dnrRuleFiles.has(entry.name)) {
          // Sanitize DNR rules
          try {
            const jsonString = new TextDecoder().decode(
              new Uint8Array(entry.data),
            );
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
          } catch (e) {
            console.error(
              `[CRXConverter] Failed to sanitize DNR rules for ${entry.name}:`,
              e,
            );
            // Fallback to original file if sanitization fails
            const stream = createInputStream(new Uint8Array(entry.data));
            zipWriter.addEntryStream(
              entry.name,
              Date.now() * 1000,
              Ci.nsIZipWriter.COMPRESSION_DEFAULT as number,
              stream,
              false,
            );
          }
        } else {
          // Copy other files as-is
          const stream = createInputStream(new Uint8Array(entry.data));
          // COMPRESSION_DEFLATE = 9
          zipWriter.addEntryStream(
            entry.name,
            Date.now() * 1000,
            Ci.nsIZipWriter.COMPRESSION_DEFAULT as number,
            stream,
            false,
          );
        }
      }

      zipWriter.close();

      // Read the output ZIP back
      const modifiedZip = readFileToArrayBuffer(outputFile);

      // Clean up output temp file
      try {
        outputFile.remove(false);
      } catch {
        // Ignore cleanup errors
      }

      return { modifiedZip, originalManifest };
    } catch (error) {
      console.error("[CRXConverter] Manifest modification failed:", error);
      // Re-throw the error so the caller can handle it
      throw error;
    }
  },
};
