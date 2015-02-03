/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "unstable"
};

const mime = "application/javascript";
const requireURI = module.uri.replace("framescript/manager.js",
                                      "toolkit/require.js");

const requireLoadURI = `data:${mime},this["Components"].utils.import("${requireURI}")`

// Loads module with given `id` into given `messageManager` via shared module loader. If `init`
// string is passed, will call module export with that name and pass frame script environment
// of the `messageManager` into it. Since module will load only once per process (which is
// once for chrome proces & second for content process) it is useful to have an init function
// to setup event listeners on each content frame.
const loadModule = (messageManager, id, allowDelayed, init) => {
  const moduleLoadURI = `${requireLoadURI}.require("${id}")`
  const uri = init ? `${moduleLoadURI}.${init}(this)` : moduleLoadURI;
  messageManager.loadFrameScript(uri, allowDelayed);
};
exports.loadModule = loadModule;
