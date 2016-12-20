/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");

let rt;
function getRuntime(type) {
  if (!rt) {
    let process = Cc["@mozilla.org/plugin/ppapi.js-process;1"].getService(Ci.nsIPPAPIJSProcess);
    Cu.import("resource://ppapi.js/ppapi-runtime.jsm");
    rt = new PPAPIRuntime(process);
    process.launch(rt.callback);
  }
  return rt;
}

addMessageListener("ppapi.js:createInstance", ({ target, data: { type, info }, objects: { pluginWindow } }) => {
  dump("ppapi.js:createInstance\n");
  let rt = getRuntime(type);
  let instance = rt.createInstance(info, content, docShell.chromeEventHandler, pluginWindow, target);
  addEventListener("unload", () => {
    rt.destroyInstance(instance);
  });
});

addEventListener("DOMContentLoaded", () => {
  // Passing an object here forces the creation of the CPOW manager in the
  // parent.
  sendRpcMessage("ppapi.js:frameLoaded", undefined, {});
});
