/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { memorySpec } = require("devtools/shared/specs/memory");
const { Task } = require("devtools/shared/task");
const protocol = require("devtools/shared/protocol");

loader.lazyRequireGetter(this, "FileUtils",
                         "resource://gre/modules/FileUtils.jsm", true);
loader.lazyRequireGetter(this, "HeapSnapshotFileUtils",
                         "devtools/shared/heapsnapshot/HeapSnapshotFileUtils");

const MemoryFront = protocol.FrontClassWithSpec(memorySpec, {
  initialize: function (client, form, rootForm = null) {
    protocol.Front.prototype.initialize.call(this, client, form);
    this._client = client;
    this.actorID = form.memoryActor;
    this.heapSnapshotFileActorID = rootForm
      ? rootForm.heapSnapshotFileActor
      : null;
    this.manage(this);
  },

  /**
   * Save a heap snapshot, transfer it from the server to the client if the
   * server and client do not share a file system, and return the local file
   * path to the heap snapshot.
   *
   * Note that this is safe to call for actors inside sandoxed child processes,
   * as we jump through the correct IPDL hoops.
   *
   * @params Boolean options.forceCopy
   *         Always force a bulk data copy of the saved heap snapshot, even when
   *         the server and client share a file system.
   *
   * @returns Promise<String>
   */
  saveHeapSnapshot: protocol.custom(Task.async(function* (options = {}) {
    const snapshotId = yield this._saveHeapSnapshotImpl();

    if (!options.forceCopy &&
        (yield HeapSnapshotFileUtils.haveHeapSnapshotTempFile(snapshotId))) {
      return HeapSnapshotFileUtils.getHeapSnapshotTempFilePath(snapshotId);
    }

    return yield this.transferHeapSnapshot(snapshotId);
  }), {
    impl: "_saveHeapSnapshotImpl"
  }),

  /**
   * Given that we have taken a heap snapshot with the given id, transfer the
   * heap snapshot file to the client. The path to the client's local file is
   * returned.
   *
   * @param {String} snapshotId
   *
   * @returns Promise<String>
   */
  transferHeapSnapshot: protocol.custom(function (snapshotId) {
    if (!this.heapSnapshotFileActorID) {
      throw new Error("MemoryFront initialized without a rootForm");
    }

    const request = this._client.request({
      to: this.heapSnapshotFileActorID,
      type: "transferHeapSnapshot",
      snapshotId
    });

    return new Promise((resolve, reject) => {
      const outFilePath =
        HeapSnapshotFileUtils.getNewUniqueHeapSnapshotTempFilePath();
      const outFile = new FileUtils.File(outFilePath);

      const outFileStream = FileUtils.openSafeFileOutputStream(outFile);
      request.on("bulk-reply", Task.async(function* ({ copyTo }) {
        yield copyTo(outFileStream);
        FileUtils.closeSafeFileOutputStream(outFileStream);
        resolve(outFilePath);
      }));
    });
  })
});

exports.MemoryFront = MemoryFront;
