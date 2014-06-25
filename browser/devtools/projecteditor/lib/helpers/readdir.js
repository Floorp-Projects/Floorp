/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

importScripts("resource://gre/modules/osfile.jsm");

/**
 * This file is meant to be loaded in a worker using:
 *   new ChromeWorker("chrome://browser/content/devtools/readdir.js");
 *
 * Read a local directory inside of a web woker
 *
 * @param {string} path
 *        window to inspect
 * @param {RegExp|string} ignore
 *        A pattern to ignore certain files.  This is
 *        called with file.name.match(ignore).
 * @param {Number} maxDepth
 *        How many directories to recurse before stopping.
 *        Directories with depth > maxDepth will be ignored.
 */
function readDir(path, ignore, maxDepth = Infinity) {
  let ret = {};

  let set = new Set();

  let info = OS.File.stat(path);
  set.add({
    path: path,
    name: info.name,
    isDir: info.isDir,
    isSymLink: info.isSymLink,
    depth: 0
  });

  for (let info of set) {
    let children = [];

    if (info.isDir && !info.isSymLink) {
      if (info.depth > maxDepth) {
        continue;
      }

      let iterator = new OS.File.DirectoryIterator(info.path);
      try {
        for (let child in iterator) {
          if (ignore && child.name.match(ignore)) {
            continue;
          }

          children.push(child.path);
          set.add({
            path: child.path,
            name: child.name,
            isDir: child.isDir,
            isSymLink: child.isSymLink,
            depth: info.depth + 1
          });
        }
      } finally {
        iterator.close();
      }
    }

    ret[info.path] = {
      name: info.name,
      isDir: info.isDir,
      isSymLink: info.isSymLink,
      depth: info.depth,
      children: children,
    };
  }

  return ret;
};

onmessage = function (event) {
  try {
    let {path, ignore, depth} = event.data;
    let message = readDir(path, ignore, depth);
    postMessage(message);
  } catch(ex) {
    console.log(ex);
  }
};


