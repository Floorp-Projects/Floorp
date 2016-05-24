/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Ci } = require("chrome");
const Services = require("Services");
const EventEmitter = require("devtools/shared/event-emitter");

loader.lazyImporter(this, "OS", "resource://gre/modules/osfile.jsm");

const HOTRELOAD_PREF = "devtools.loader.hotreload";

function resolveResourcePath(uri) {
  const handler = Services.io.getProtocolHandler("resource")
        .QueryInterface(Ci.nsIResProtocolHandler);
  const resolved = handler.resolveURI(Services.io.newURI(uri, null, null));
  return resolved.replace(/file:\/\//, "");
}

function findSourceDir(path) {
  if (path === "" || path === "/") {
    return Promise.resolve(null);
  }

  return OS.File.exists(
    OS.Path.join(path, "devtools/client/shared/file-watcher.js")
  ).then(exists => {
    if (exists) {
      return path;
    }
    return findSourceDir(OS.Path.dirname(path));
  });
}

let worker = null;
const onPrefChange = function () {
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
  if (Services.prefs.getBoolPref(HOTRELOAD_PREF) && !worker) {
    const devtoolsPath = resolveResourcePath("resource://devtools")
      .replace(/\/$/, "");
    const searchPoint = OS.Path.dirname(OS.Path.dirname(devtoolsPath));
    findSourceDir(searchPoint)
      .then(srcPath => {
        const rootPath = srcPath ? OS.Path.join(srcPath, "devtools")
                                 : devtoolsPath;
        const watchPath = OS.Path.join(rootPath, "client");
        const { watchFiles } = require("devtools/client/shared/file-watcher");
        worker = watchFiles(watchPath, path => {
          let relativePath = path.replace(rootPath + "/", "");
          module.exports.emit("file-changed", relativePath, path);
        });
      });
  } else if (worker) {
    worker.terminate();
    worker = null;
  }
};

Services.prefs.addObserver(HOTRELOAD_PREF, {
  observe: onPrefChange
}, false);
onPrefChange();

EventEmitter.decorate(module.exports);
