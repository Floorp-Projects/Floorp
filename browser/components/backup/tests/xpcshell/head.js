/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

"use strict";

const { BackupService } = ChromeUtils.importESModule(
  "resource:///modules/backup/BackupService.sys.mjs"
);

const { BackupResource } = ChromeUtils.importESModule(
  "resource:///modules/backup/BackupResource.sys.mjs"
);

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const BYTES_IN_KB = 1000;

do_get_profile();

/**
 * Some fake backup resource classes to test with.
 */
class FakeBackupResource1 extends BackupResource {
  static get key() {
    return "fake1";
  }
  measure() {}
}

/**
 * Another fake backup resource class to test with.
 */
class FakeBackupResource2 extends BackupResource {
  static get key() {
    return "fake2";
  }
  measure() {}
}

/**
 * Yet another fake backup resource class to test with.
 */
class FakeBackupResource3 extends BackupResource {
  static get key() {
    return "fake3";
  }
  measure() {}
}

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
