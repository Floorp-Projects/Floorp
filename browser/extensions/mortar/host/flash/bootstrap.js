/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");

function sandboxScript(sandbox)
{
  dump("sandboxScript " + sandbox.pluginElement + "\n");
  Components.classes["@mozilla.org/moz/jssubscript-loader;1"].getService(Components.interfaces.mozIJSSubScriptLoader).loadSubScript("resource://ppapiflash.js/ppapi-content-sandbox.js", sandbox);
}

let plugins;
function startup(data) {
  dump(">>>STARTED!!!\n");

  let root = data.installPath.parent.parent;
  let rpclib = root.clone();
  let pluginlib = root.clone();
  let os = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS;
  if (os == "Darwin") {
    rpclib.appendRelativePath("ppapi/out/rpc.dylib");
    pluginlib.appendRelativePath("plugin/PepperFlashPlayer-debug");
  } else if (os == "Linux") {
    rpclib.appendRelativePath("ppapi/out/rpc.so");
    pluginlib.appendRelativePath("plugin/libpepflashplayer.so");
  } else if (os == "WINNT") {
    rpclib.appendRelativePath("ppapi\\out\\rpc.dll");
    pluginlib.appendRelativePath("plugin\\pepflashplayer.dll");
  } else {
    throw("Don't know the path to the libraries for this OS!");
  }
  rpclib = rpclib.path;
  pluginlib = pluginlib.path;

  let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  let plugin = pluginHost.registerFakePlugin({
    handlerURI: "chrome://ppapiflash.js/content/viewer.html",
    mimeEntries: [{ type: "application/x-shockwave-flash", extension: "swf" }],
    name: "Shockwave Flash",
    niceName: "PPAPI Flash plugin",
    description: "10",
    version: "1.0",
    sandboxScript : `(${sandboxScript.toSource()})(this);`,
    ppapiProcessArgs: [ rpclib, pluginlib ],
  });
  plugin.enabledState = Ci.nsIPluginTag.STATE_ENABLED;

  let rng = Cc["@mozilla.org/security/random-generator;1"].createInstance(Ci.nsIRandomGenerator);
  Services.ppmm.addMessageListener("ppapi.js:generateRandomBytes", ({ data }) => {
    return rng.generateRandomBytes(data);
  });

  let moduleLocalFiles = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
  moduleLocalFiles.append("Flash PPAPI Data");
  try {
    moduleLocalFiles.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  } catch (e) {
    if (e.result != Cr.NS_ERROR_FILE_ALREADY_EXISTS) {
      throw e;
    }
  }
  Services.ppmm.addMessageListener("ppapiflash.js:getModuleLocalFilesPath", () => {
    return moduleLocalFiles.path;
  });

  dump("<<<STARTED!!!\n");
}

function shutdown() {
  dump("SHUTDOWN!!!\n");
}
