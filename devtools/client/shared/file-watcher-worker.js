/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* eslint-env worker */
/* global OS */
importScripts("resource://gre/modules/osfile.jsm");

const modifiedTimes = new Map();

function findSourceDir(path) {
  if (path === "" || path === "/") {
    return null;
  } else if (OS.File.exists(
    OS.Path.join(path, "devtools/client/shared/file-watcher.js")
  )) {
    return path;
  }
  return findSourceDir(OS.Path.dirname(path));
}

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

onmessage = function(event) {
  const { path, fileRegex } = event.data;
  const devtoolsPath = event.data.devtoolsPath.replace(/\/$/, "");

  // We need to figure out a src dir to watch. These are the actual
  // files the user is working with, not the files in the obj dir. We
  // do this by walking up the filesystem and looking for the devtools
  // directories, and falling back to the raw path. This means none of
  // this will work for users who store their obj dirs outside of the
  // src dir.
  //
  // We take care not to mess with the `devtoolsPath` if that's what
  // we end up using, because it might be intentionally mapped to a
  // specific place on the filesystem for loading devtools externally.
  //
  // `devtoolsPath` is currently the devtools directory inside of the
  // obj dir, and we search for `devtools/client`, so go up 2 levels
  // to skip that devtools dir and start searching for the src dir.
  const searchPoint = OS.Path.dirname(OS.Path.dirname(devtoolsPath));
  const srcPath = findSourceDir(searchPoint);
  const rootPath = srcPath ? OS.Path.join(srcPath, "devtools") : devtoolsPath;
  const watchPath = OS.Path.join(rootPath, path.replace(/^devtools\//, ""));

  const info = OS.File.stat(watchPath);
  if (!info.isDir) {
    throw new Error("Watcher expects a directory as root path");
  }

  // We get a list of all the files upfront, which means we don't
  // support adding new files. But you need to rebuild Firefox when
  // adding a new file anyway.
  const files = gatherFiles(watchPath, fileRegex || /.*/);

  // Every second, scan for file changes by stat-ing each of them and
  // comparing modification time.
  setInterval(() => {
    scanFiles(files, changedFile => {
      postMessage({ fullPath: changedFile,
                    relativePath: changedFile.replace(rootPath + "/", "") });
    });
  }, 1000);
};

