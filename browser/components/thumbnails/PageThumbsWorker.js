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
      case "removeFiles":
        data.result = this.removeFiles(msg);
        break;
      case "getFilesInDirectory":
        data.result = this.getFilesInDirectory(msg);
        break;
      default:
        data.result = false;
        data.detail = "message not understood";
        break;
    }

    self.postMessage(data);
  },

  getFilesInDirectory: function Worker_getFilesInDirectory(msg) {
    let iter = new OS.File.DirectoryIterator(msg.path);
    let entries = [];

    for (let entry in iter) {
      if (!entry.isDir && !entry.isSymLink) {
        entries.push(entry.name);
      }
    }

    iter.close();
    return entries;
  },

  removeFile: function Worker_removeFile(msg) {
    try {
      OS.File.remove(msg.path);
      return true;
    } catch (e) {
      return false;
    }
  },

  removeFiles: function Worker_removeFiles(msg) {
    for (let file of msg.paths) {
      try {
        OS.File.remove(file);
      } catch (e) {
        // We couldn't remove the file for some reason.
        // Let's just continue with the next one.
      }
    }
    return true;
  }
};

self.onmessage = PageThumbsWorker.handleMessage.bind(PageThumbsWorker);
