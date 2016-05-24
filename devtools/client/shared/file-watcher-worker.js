/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* eslint-env worker */
/* global OS */
importScripts("resource://gre/modules/osfile.jsm");

const modifiedTimes = new Map();

function gatherFiles(path, fileRegex) {
  let files = [];
  const iterator = new OS.File.DirectoryIterator(path);

  try {
    for (let child in iterator) {
      // Don't descend into test directories. Saves us some time and
      // there's no reason to.
      if (child.isDir && !child.path.endsWith("/test")) {
        files = files.concat(gatherFiles(child.path, fileRegex));
      } else if (child.path.match(fileRegex)) {
        let info;
        try {
          info = OS.File.stat(child.path);
        } catch (e) {
          // Just ignore it.
          continue;
        }

        files.push(child.path);
        modifiedTimes.set(child.path, info.lastModificationDate.getTime());
      }
    }
  } finally {
    iterator.close();
  }

  return files;
}

function scanFiles(files, onChangedFile) {
  files.forEach(file => {
    let info;
    try {
      info = OS.File.stat(file);
    } catch (e) {
      // Just ignore it. It was probably deleted.
      return;
    }

    const lastTime = modifiedTimes.get(file);

    if (info.lastModificationDate.getTime() > lastTime) {
      modifiedTimes.set(file, info.lastModificationDate.getTime());
      onChangedFile(file);
    }
  });
}

onmessage = function (event) {
  const { path, fileRegex } = event.data;

  const info = OS.File.stat(path);
  if (!info.isDir) {
    throw new Error("Watcher expects a directory as root path");
  }

  // We get a list of all the files upfront, which means we don't
  // support adding new files. But you need to rebuild Firefox when
  // adding a new file anyway.
  const files = gatherFiles(path, fileRegex || /.*/);

  // Every second, scan for file changes by stat-ing each of them and
  // comparing modification time.
  setInterval(() => {
    scanFiles(files, changedFile => {
      postMessage({ path: changedFile });
    });
  }, 1000);
};
