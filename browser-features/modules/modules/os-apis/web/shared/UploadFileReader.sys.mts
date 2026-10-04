/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export interface PreparedUploadFile {
  fileData: number[];
  fileName: string;
}

interface UploadFileReaderDependencies {
  exists(path: string): Promise<boolean>;
  read(path: string): Promise<Uint8Array>;
  filename(path: string): string;
}

const DEFAULT_DEPENDENCIES: UploadFileReaderDependencies = {
  exists: (path) => IOUtils.exists(path),
  read: (path) => IOUtils.read(path),
  filename: (path) => PathUtils.filename(path),
};

/**
 * Read an upload selected by a trusted parent-process caller.
 *
 * File paths must never cross from an untrusted web content process into a
 * privileged IOUtils call. Services invoke this helper before sending the
 * resulting bytes to NRWebScraperChild.
 */
export async function prepareUploadFile(
  filePath: string,
  dependencies: UploadFileReaderDependencies = DEFAULT_DEPENDENCIES,
): Promise<PreparedUploadFile | null> {
  if (typeof filePath !== "string" || filePath.trim().length === 0) {
    return null;
  }

  try {
    if (!(await dependencies.exists(filePath))) {
      return null;
    }

    const fileName = dependencies.filename(filePath);
    if (!fileName) {
      return null;
    }

    const fileData = await dependencies.read(filePath);
    return {
      fileData: Array.from(fileData),
      fileName,
    };
  } catch (error) {
    console.error("[WebScraper] Failed to prepare upload file:", error);
    return null;
  }
}
