/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Heap snapshots are always saved in the temp directory, and have a regular
// naming convention. This module provides helpers for working with heap
// snapshot files in a safe manner. Because we attempt to avoid unnecessary
// copies of the heap snapshot files by checking the local filesystem for a heap
// snapshot file with the given snapshot id, we want to ensure that we are only
// attempting to open heap snapshot files and not `~/.ssh/id_rsa`, for
// example. Therefore, the RDP only talks about snapshot ids, or transfering the
// bulk file data. A file path can be recovered from a snapshot id, which allows
// one to check for the presence of the heap snapshot file on the local file
// system, but we don't have to worry about opening arbitrary files.
//
// The heap snapshot file path conventions permits the following forms:
//
//     $TEMP_DIRECTORY/XXXXXXXXXX.fxsnapshot
//     $TEMP_DIRECTORY/XXXXXXXXXX-XXXXX.fxsnapshot
//
// Where the strings of "X" are zero or more digits.

"use strict";

const { Ci } = require("chrome");
loader.lazyRequireGetter(this, "FileUtils",
                         "resource://gre/modules/FileUtils.jsm", true);
loader.lazyRequireGetter(this, "OS", "resource://gre/modules/osfile.jsm", true);

function getHeapSnapshotFileTemplate() {
  return OS.Path.join(OS.Constants.Path.tmpDir, `${Date.now()}.fxsnapshot`);
}

/**
 * Get a unique temp file path for a new heap snapshot. The file is guaranteed
 * not to exist before this call.
 *
 * @returns String
 */
exports.getNewUniqueHeapSnapshotTempFilePath = function() {
  const file = new FileUtils.File(getHeapSnapshotFileTemplate());
  // The call to createUnique will append "-N" after the leaf name (but before
  // the extension) until a new file is found and create it. This guarantees we
  // won't accidentally choose the same file twice.
  file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, 0o666);
  return file.path;
};

function isValidSnapshotFileId(snapshotId) {
  return /^\d+(\-\d+)?$/.test(snapshotId);
}

/**
 * Get the file path for the given snapshot id.
 *
 * @param {String} snapshotId
 *
 * @returns String | null
 */
exports.getHeapSnapshotTempFilePath = function(snapshotId) {
  // Don't want anyone sneaking "../../../.." strings into the snapshot id and
  // trying to make us open arbitrary files.
  if (!isValidSnapshotFileId(snapshotId)) {
    return null;
  }
  return OS.Path.join(OS.Constants.Path.tmpDir, snapshotId + ".fxsnapshot");
};

/**
 * Return true if we have the heap snapshot file for the given snapshot id on
 * the local file system. False is returned otherwise.
 *
 * @returns Promise<Boolean>
 */
exports.haveHeapSnapshotTempFile = function(snapshotId) {
  const path = exports.getHeapSnapshotTempFilePath(snapshotId);
  if (!path) {
    return Promise.resolve(false);
  }

  return OS.File.stat(path).then(() => true,
                                 () => false);
};
