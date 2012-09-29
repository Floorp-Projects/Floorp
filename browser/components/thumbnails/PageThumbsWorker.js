/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A worker dedicated for the I/O component of PageThumbs storage.
 *
 * Do not rely on the API of this worker. In a future version, it might be
 * fully replaced by a OS.File global I/O worker.
 */

"use strict";

importScripts("resource://gre/modules/osfile.jsm");

let PageThumbsWorker = {
  handleMessage: function Worker_handleMessage(aEvent) {
    let msg = aEvent.data;
    let data = {result: null, data: null};

    switch (msg.type) {
      case "removeFile":
        data.result = this.removeFile(msg);
        break;
      case "expireFilesInDirectory":
        data.result = this.expireFilesInDirectory(msg);
        break;
      default:
        data.result = false;
        data.detail = "message not understood";
        break;
    }

    self.postMessage(data);
  },

  removeFile: function Worker_removeFile(msg) {
    try {
      OS.File.remove(msg.path);
      return true;
    } catch (e) {
      return false;
    }
  },

  expireFilesInDirectory: function Worker_expireFilesInDirectory(msg) {
    let entries = this.getFileEntriesInDirectory(msg.path, msg.filesToKeep);
    let limit = Math.max(msg.minChunkSize, Math.round(entries.length / 2));

    for (let entry of entries) {
      this.removeFile(entry);

      // Check if we reached the limit of files to remove.
      if (--limit <= 0) {
        break;
      }
    }

    return true;
  },

  getFileEntriesInDirectory:
  function Worker_getFileEntriesInDirectory(aPath, aSkipFiles) {
    let skip = new Set(aSkipFiles);
    let iter = new OS.File.DirectoryIterator(aPath);

    return [entry
            for (entry in iter)
            if (!entry.isDir && !entry.isSymLink && !skip.has(entry.name))];
  }
};

self.onmessage = PageThumbsWorker.handleMessage.bind(PageThumbsWorker);
