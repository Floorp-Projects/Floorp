/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");

const messageGenerateRandomBytes = "ppapi.js:generateRandomBytes";
const handlerURI = "chrome://ppapipdf.js/content/viewer.html";
const prefPdfiumEnable = "pdfium.enabled";

function sandboxScript(sandbox)
{
  dump("sandboxScript " + sandbox.pluginElement + "\n");
  Components.classes["@mozilla.org/moz/jssubscript-loader;1"].getService(Components.interfaces.mozIJSSubScriptLoader).loadSubScript("resource://ppapipdf.js/ppapi-content-sandbox.js", sandbox);
}

function prefObserver(aSubject, aTopic, aData) {
  if (Services.prefs.getBoolPref(prefPdfiumEnable)) {
    pdfium.init();
  } else {
    pdfium.uninit();
  }
}

let bootstrapData;
let pdfium = {
  init() {
    let root = bootstrapData.installPath.parent.parent;
    let rpclib = root.clone();
    let pluginlib = root.clone();
    let os = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS;
    if (os == "Darwin") {
      rpclib.appendRelativePath("ppapi/out/rpc.dylib");
      pluginlib.appendRelativePath("plugin/libpepperpdfium.dylib");
    } else if (os == "Linux") {
      rpclib.appendRelativePath("ppapi/out/rpc.so");
      pluginlib.appendRelativePath("plugin/libpepperpdfium.so");
    } else if (os == "WINNT") {
      rpclib.appendRelativePath("ppapi\\out\\rpc.dll");
      pluginlib.appendRelativePath("plugin\\pepperpdfium.dll");
    } else {
      throw("Don't know the path to the libraries for this OS!");
    }
    rpclib = rpclib.path;
    pluginlib = pluginlib.path;

    let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
    let plugin = pluginHost.registerFakePlugin({
      handlerURI: handlerURI,
      mimeEntries: [
        { type: "application/pdf", extension: "pdf" },
        { type: "application/vnd.adobe.pdf", extension: "pdf" },
        { type: "application/vnd.adobe.pdfxml", extension: "pdfxml" },
        { type: "application/vnd.adobe.x-mars", extension: "mars" },
        { type: "application/vnd.adobe.xdp+xml", extension: "xdp" },
        { type: "application/vnd.adobe.xfdf", extension: "xfdf" },
        { type: "application/vnd.adobe.xfd+xml", extension: "xfd" },
        { type: "application/vnd.fdf", extension: "fdf" },
      ],
      name: "PPAPI PDF plugin",
      niceName: "PPAPI PDF plugin",
      version: "1.0",
      sandboxScript : `(${sandboxScript.toSource()})(this);`,
      ppapiProcessArgs: [ rpclib, pluginlib ],
    });
    plugin.enabledState = Ci.nsIPluginTag.STATE_ENABLED;
  },
  uninit() {
    let pluginHost = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
    pluginHost.unregisterFakePlugin(handlerURI);
  },
  generateRandomBytesListener(data) {
    let rng = Cc["@mozilla.org/security/random-generator;1"].createInstance(Ci.nsIRandomGenerator);
    return rng.generateRandomBytes(data);
  },
}

function startup(aData) {
  dump(">>>STARTED!!!\n");
  bootstrapData = aData;
  if (Services.prefs.getBoolPref(prefPdfiumEnable)) {
    pdfium.init();
  }
  Services.prefs.addObserver(prefPdfiumEnable, prefObserver);
  Services.ppmm.addMessageListener(messageGenerateRandomBytes, pdfium.generateRandomBytesListener);
  dump("<<<STARTED!!!\n");
}
function shutdown() {
  dump("SHUTDOWN!!!\n");
  pdfium.uninit();
  Services.prefs.removeObserver(prefPdfiumEnable, prefObserver);
  Services.ppmm.removeMessageListener(messageGenerateRandomBytes, pdfium.generateRandomBytesListener);
}
