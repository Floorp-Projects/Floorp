/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol.js");
const {
  heapSnapshotFileSpec,
} = require("resource://devtools/shared/specs/heap-snapshot-file.js");

loader.lazyRequireGetter(
  this,
  "DevToolsUtils",
  "resource://devtools/shared/DevToolsUtils.js"
);
loader.lazyRequireGetter(
  this,
  "HeapSnapshotFileUtils",
  "resource://devtools/shared/heapsnapshot/HeapSnapshotFileUtils.js"
);

/**
 * The HeapSnapshotFileActor handles transferring heap snapshot files from the
 * server to the client. This has to be a global actor in the parent process
 * because child processes are sandboxed and do not have access to the file
 * system.
 */
exports.HeapSnapshotFileActor = class HeapSnapshotFileActor extends Actor {
  constructor(conn, parent) {
    super(conn, heapSnapshotFileSpec);

    if (
      Services.appinfo.processType !== Services.appinfo.PROCESS_TYPE_DEFAULT
    ) {
      const err = new Error(
        "Attempt to create a HeapSnapshotFileActor in a " +
          "child process! The HeapSnapshotFileActor *MUST* " +
          "be in the parent process!"
      );
      DevToolsUtils.reportException("HeapSnapshotFileActor's constructor", err);
    }
  }

  /**
   * @see MemoryFront.prototype.transferHeapSnapshot
   */
  async transferHeapSnapshot(snapshotId) {
    const snapshotFilePath =
      HeapSnapshotFileUtils.getHeapSnapshotTempFilePath(snapshotId);
    if (!snapshotFilePath) {
      throw new Error(`No heap snapshot with id: ${snapshotId}`);
    }

    const streamPromise = DevToolsUtils.openFileStream(snapshotFilePath);

    const { size } = await IOUtils.stat(snapshotFilePath);
    const bulkPromise = this.conn.startBulkSend({
      actor: this.actorID,
      type: "heap-snapshot",
      length: size,
    });

    const [bulk, stream] = await Promise.all([bulkPromise, streamPromise]);

    try {
      await bulk.copyFrom(stream);
    } finally {
      stream.close();
    }
  }
};
