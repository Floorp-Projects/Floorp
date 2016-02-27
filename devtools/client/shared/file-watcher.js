/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Ci, ChromeWorker } = require("chrome");
const Services = require("Services");
const EventEmitter = require("devtools/shared/event-emitter");

const HOTRELOAD_PREF = "devtools.loader.hotreload";

function resolveResourcePath(uri) {
  const handler = Services.io.getProtocolHandler("resource")
        .QueryInterface(Ci.nsIResProtocolHandler);
  const resolved = handler.resolveURI(Services.io.newURI(uri, null, null));
  return resolved.replace(/file:\/\//, "");
}

function watchFiles(path, onFileChanged) {
  if (!path.startsWith("devtools/")) {
    throw new Error("`watchFiles` expects a devtools path");
  }

  const watchWorker = new ChromeWorker(
    "resource://devtools/client/shared/file-watcher-worker.js"
  );

  watchWorker.onmessage = event => {
    // We need to turn a local path back into a resource URI (or
    // chrome). This means that this system will only work when built
    // files are symlinked, so that these URIs actually read from
    // local sources. There might be a better way to do this.
    const { relativePath, fullPath } = event.data;
    onFileChanged(relativePath, fullPath);
  };

  watchWorker.postMessage({
    path: path,
    // We must do this here because we can't access the needed APIs in
    // a worker.
    devtoolsPath: resolveResourcePath("resource://devtools"),
    fileRegex: /\.(js|css|svg|png)$/ });
  return watchWorker;
}

EventEmitter.decorate(module.exports);

let watchWorker;
function onPrefChange() {
  if (Services.prefs.getBoolPref(HOTRELOAD_PREF) && !watchWorker) {
    watchWorker = watchFiles("devtools/client", (relativePath, fullPath) => {
      module.exports.emit("file-changed", relativePath, fullPath);
    });
  } else if (watchWorker) {
    watchWorker.terminate();
    watchWorker = null;
  }
}

Services.prefs.addObserver(HOTRELOAD_PREF, {
  observe: onPrefChange
}, false);

onPrefChange();
