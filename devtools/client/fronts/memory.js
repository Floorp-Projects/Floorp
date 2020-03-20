/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { memorySpec } = require("devtools/shared/specs/memory");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");

loader.lazyRequireGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm",
  true
);
loader.lazyRequireGetter(
  this,
  "HeapSnapshotFileUtils",
  "devtools/shared/heapsnapshot/HeapSnapshotFileUtils"
);

class MemoryFront extends FrontClassWithSpec(memorySpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);
    this._client = client;
    this.heapSnapshotFileActorID = null;

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "memoryActor";
  }

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
   * @params {Object|undefined} options.boundaries
   *         The boundaries for the heap snapshot. See
   *         ChromeUtils.webidl for more details.
   *
   * @returns Promise<String>
   */
  async saveHeapSnapshot(options = {}) {
    const snapshotId = await super.saveHeapSnapshot(options.boundaries);

    if (
      !options.forceCopy &&
      (await HeapSnapshotFileUtils.haveHeapSnapshotTempFile(snapshotId))
    ) {
      return HeapSnapshotFileUtils.getHeapSnapshotTempFilePath(snapshotId);
    }

    return this.transferHeapSnapshot(snapshotId);
  }

  /**
   * Given that we have taken a heap snapshot with the given id, transfer the
   * heap snapshot file to the client. The path to the client's local file is
   * returned.
   *
   * @param {String} snapshotId
   *
   * @returns Promise<String>
   */
  async transferHeapSnapshot(snapshotId) {
    if (!this.heapSnapshotFileActorID) {
      const form = await this._client.mainRoot.rootForm;
      this.heapSnapshotFileActorID = form.heapSnapshotFileActor;
    }

    try {
      const request = this._client.request({
        to: this.heapSnapshotFileActorID,
        type: "transferHeapSnapshot",
        snapshotId,
      });

      const outFilePath = HeapSnapshotFileUtils.getNewUniqueHeapSnapshotTempFilePath();
      const outFile = new FileUtils.File(outFilePath);
      const outFileStream = FileUtils.openSafeFileOutputStream(outFile);

      // This request is a bulk request. That's why the result of the request is
      // an object with the `copyTo` function that can transfer the data to
      // another stream.
      // See devtools/shared/transport/transport.js to know more about this mode.
      const { copyTo } = await request;
      await copyTo(outFileStream);

      FileUtils.closeSafeFileOutputStream(outFileStream);
      return outFilePath;
    } catch (e) {
      if (e.error) {
        // This isn't a real error, rather this is a message coming from the
        // server. So let's throw a real error instead.
        throw new Error(
          `The server's actor threw an error: (${e.error}) ${e.message}`
        );
      }

      // Otherwise, rethrow the error
      throw e;
    }
  }
}

exports.MemoryFront = MemoryFront;
registerFront(MemoryFront);
