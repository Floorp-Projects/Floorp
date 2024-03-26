/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

"use strict";

const BYTES_IN_KB = 1000;

/**
 * Create a file of a given size in kilobytes.
 *
 * @param {string} path the path where the file will be created.
 * @param {number} sizeInKB size file in Kilobytes.
 * @returns {Promise<undefined>}
 */
async function createKilobyteSizedFile(path, sizeInKB) {
  let bytes = new Uint8Array(sizeInKB * BYTES_IN_KB);
  await IOUtils.write(path, bytes);
}

/**
 * Remove a file if it exists and is unlocked.
 *
 * @param {string} path path to the file to remove.
 */
async function maybeRemoveFile(path) {
  if (await IOUtils.exists(path)) {
    return;
  }

  try {
    await IOUtils.remove(path);
  } catch (error) {
    // Sometimes remove() throws when the file is not unlocked soon
    // enough.
    if (error.name == "NS_ERROR_FILE_IS_LOCKED") {
      return;
    }
    throw error;
  }
}
