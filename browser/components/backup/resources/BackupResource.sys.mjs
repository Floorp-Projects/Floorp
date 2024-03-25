/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

// Convert from bytes to kilobytes (not kibibytes).
const BYTES_IN_KB = 1000;

/**
 * An abstract class representing a set of data within a user profile
 * that can be persisted to a separate backup archive file, and restored
 * to a new user profile from that backup archive file.
 */
export class BackupResource {
  /**
   * This must be overridden to return a simple string identifier for the
   * resource, for example "places" or "extensions". This key is used as
   * a unique identifier for the resource.
   *
   * @type {string}
   */
  static get key() {
    throw new Error("BackupResource::key needs to be overridden.");
  }

  /**
   * Get the size of a file.
   *
   * @param {string} filePath - path to a file.
   * @returns {Promise<number|null>} - the size of the file in kilobytes, or null if the
   * file does not exist, the path is a directory or the size is unknown.
   */
  static async getFileSize(filePath) {
    if (!(await IOUtils.exists(filePath))) {
      return null;
    }

    let { size } = await IOUtils.stat(filePath);

    if (size < 0) {
      return null;
    }

    let sizeInKb = Math.ceil(size / BYTES_IN_KB);
    // Make the measurement fuzzier by rounding to the nearest 10kb.
    let nearestTenthKb = Math.round(sizeInKb / 10) * 10;

    return Math.max(nearestTenthKb, 1);
  }

  /**
   * Get the total size of a directory.
   *
   * @param {string} directoryPath - path to a directory.
   * @returns {Promise<number|null>} - the size of all descendants of the directory in kilobytes, or null if the
   * directory does not exist, the path is not a directory or the size is unknown.
   */
  static async getDirectorySize(directoryPath) {
    if (!(await IOUtils.exists(directoryPath))) {
      return null;
    }

    let { type } = await IOUtils.stat(directoryPath);

    if (type != "directory") {
      return null;
    }

    let children = await IOUtils.getChildren(directoryPath, {
      ignoreAbsent: true,
    });

    let size = 0;
    for (const childFilePath of children) {
      let { size: childSize, type: childType } = await IOUtils.stat(
        childFilePath
      );

      if (childSize >= 0) {
        let sizeInKb = Math.ceil(childSize / BYTES_IN_KB);
        // Make the measurement fuzzier by rounding to the nearest 10kb.
        let nearestTenthKb = Math.round(sizeInKb / 10) * 10;
        size += Math.max(nearestTenthKb, 1);
      }

      if (childType == "directory") {
        let childDirectorySize = await this.getDirectorySize(childFilePath);
        if (Number.isInteger(childDirectorySize)) {
          size += childDirectorySize;
        }
      }
    }

    return size;
  }

  constructor() {}

  /**
   * This must be overridden to record telemetry on the size of any
   * data associated with this BackupResource.
   *
   * @param {string} profilePath - path to a profile directory.
   * @returns {Promise<undefined>}
   */
  // eslint-disable-next-line no-unused-vars
  async measure(profilePath) {
    throw new Error("BackupResource::measure needs to be overridden.");
  }
}
