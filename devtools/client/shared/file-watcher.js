/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { ChromeWorker } = require("chrome");

function watchFiles(path, onFileChanged) {
  const watchWorker = new ChromeWorker(
    "resource://devtools/client/shared/file-watcher-worker.js"
  );

  watchWorker.onmessage = event => {
    // We need to turn a local path back into a resource URI (or
    // chrome). This means that this system will only work when built
    // files are symlinked, so that these URIs actually read from
    // local sources. There might be a better way to do this.
    const { path: newPath } = event.data;
    onFileChanged(newPath);
  };

  watchWorker.postMessage({
    path,
    fileRegex: /\.(js|css|svg|png)$/
  });
  return watchWorker;
}
exports.watchFiles = watchFiles;
