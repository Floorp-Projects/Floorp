/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const { method, Arg } = protocol;
const Services = require("Services");
const { Task } = require("devtools/shared/task");

loader.lazyRequireGetter(this, "DevToolsUtils",
                         "devtools/shared/DevToolsUtils");
loader.lazyRequireGetter(this, "OS", "resource://gre/modules/osfile.jsm", true);
loader.lazyRequireGetter(this, "HeapSnapshotFileUtils",
                         "devtools/shared/heapsnapshot/HeapSnapshotFileUtils");

/**
 * The HeapSnapshotFileActor handles transferring heap snapshot files from the
 * server to the client. This has to be a global actor in the parent process
 * because child processes are sandboxed and do not have access to the file
 * system.
 */
exports.HeapSnapshotFileActor = protocol.ActorClass({
  typeName: "heapSnapshotFile",

  initialize: function (conn, parent) {
    if (Services.appInfo &&
        (Services.appInfo.processType !==
         Services.appInfo.PROCESS_TYPE_DEFAULT)) {
      const err = new Error("Attempt to create a HeapSnapshotFileActor in a " +
                            "child process! The HeapSnapshotFileActor *MUST* " +
                            "be in the parent process!");
      DevToolsUtils.reportException(
        "HeapSnapshotFileActor.prototype.initialize", err);
      return;
    }

    protocol.Actor.prototype.initialize.call(this, conn, parent);
  },

  /**
   * @see MemoryFront.prototype.transferHeapSnapshot
   */
  transferHeapSnapshot: method(Task.async(function* (snapshotId) {
    const snapshotFilePath =
          HeapSnapshotFileUtils.getHeapSnapshotTempFilePath(snapshotId);
    if (!snapshotFilePath) {
      throw new Error(`No heap snapshot with id: ${snapshotId}`);
    }

    const streamPromise = DevToolsUtils.openFileStream(snapshotFilePath);

    const { size } = yield OS.File.stat(snapshotFilePath);
    const bulkPromise = this.conn.startBulkSend({
      actor: this.actorID,
      type: "heap-snapshot",
      length: size
    });

    const [bulk, stream] = yield Promise.all([bulkPromise, streamPromise]);

    try {
      yield bulk.copyFrom(stream);
    } finally {
      stream.close();
    }
  }), {
    request: {
      snapshotId: Arg(0, "string")
    }
  }),

});
