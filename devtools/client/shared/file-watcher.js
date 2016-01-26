/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Ci, ChromeWorker } = require("chrome");
const { Services } = require("resource://gre/modules/Services.jsm");
const EventEmitter = require("devtools/shared/event-emitter");

const HOTRELOAD_PREF = "devtools.loader.hotreload";

function resolveResourceURI(uri) {
  const handler = Services.io.getProtocolHandler("resource")
        .QueryInterface(Ci.nsIResProtocolHandler);
  return handler.resolveURI(Services.io.newURI(uri, null, null));
}

function watchFiles(path, onFileChanged) {
  if (!path.startsWith("devtools/")) {
    throw new Error("`watchFiles` expects a devtools path");
  }

  // We need to figure out a local path to watch. We start with
  // whatever devtools points to.
  let resolvedRootURI = resolveResourceURI("resource://devtools");
  if (resolvedRootURI.match(/\/obj\-.*/)) {
    // Move from the built directory to the user's local files
    resolvedRootURI = resolvedRootURI.replace(/\/obj\-.*/, "") + "/devtools";
  }
  resolvedRootURI = resolvedRootURI.replace(/^file:\/\//, "");
  const localURI = resolvedRootURI + "/" + path.replace(/^devtools\//, "");

  const watchWorker = new ChromeWorker(
    "resource://devtools/client/shared/file-watcher-worker.js"
  );

  watchWorker.onmessage = event => {
    // We need to turn a local path back into a resource URI (or
    // chrome). This means that this system will only work when built
    // files are symlinked, so that these URIs actually read from
    // local sources. There might be a better way to do this.
    const relativePath = event.data.replace(resolvedRootURI + "/", "");
    if (relativePath.startsWith("client/themes")) {
      onFileChanged(relativePath.replace("client/themes",
                                         "chrome://devtools/skin"));
    }
    onFileChanged("resource://devtools/" + relativePath);
  };

  watchWorker.postMessage({ path: localURI, fileRegex: /\.(js|css)$/ });
  return watchWorker;
}

EventEmitter.decorate(module.exports);

let watchWorker;
function onPrefChange() {
  if (Services.prefs.getBoolPref(HOTRELOAD_PREF) && !watchWorker) {
    watchWorker = watchFiles("devtools/client", changedFile => {
      module.exports.emit("file-changed", changedFile);
    });
  }
  else if(watchWorker) {
    watchWorker.terminate();
    watchWorker = null;
  }
}

Services.prefs.addObserver(HOTRELOAD_PREF, {
  observe: onPrefChange
}, false);

onPrefChange();
