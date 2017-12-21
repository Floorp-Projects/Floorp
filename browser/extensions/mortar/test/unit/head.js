"use strict";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
// For the following 5 lines of codes, we redirect the
// path of the "ppapi.js" in addon to the exact file path.
Components.utils.import("resource://gre/modules/NetUtil.jsm");
let resHandler = Services.io.getProtocolHandler("resource")
                         .QueryInterface(Components.interfaces.nsISubstitutingProtocolHandler);
let dataURI = NetUtil.newURI(do_get_file("."));
resHandler.setSubstitution("ppapi.js", dataURI);

// Load the script
load("ppapi-runtime.jsm");

let instanceId = 1;
let url = "http://example.com";
let info = {
    documentURL: "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai",
    url,
    setupJSInstanceObject: false,
    isFullFrame: false, //pluginElement.ownerDocument.mozSyntheticDocument,
    arguments: {
      keys: ["src", "full-frame", "top-level-url"],
      values: [url, "", url],
    },
};

// Head.js is a shared file for all the test_ppb***.js.
// Right now, window, process and MessageManager are base classes here.
// Fill in the classes when you need the functions of them
// and add more mocked classses if you need them in
// ppapi-runtime.jsm for your tests.
class Mock_Window {}
class Mock_Process {}
class Mock_MessageManager {
  addMessageListener () {
  }
}

// Here the new PPAPIRuntime, Call_PpbFunc and new PPAPIInstance are the
// core part to invoke codes in ppapi-runtime.jsm.
let rt = new PPAPIRuntime(new Mock_Process());

function Call_PpbFunc(obj) {
  if (!obj || !obj.__interface || !obj.__version || !obj.__method) {
    ok(false, 'invalid JSON');
  }
  let fn = obj.__interface + "_" + obj.__method;
  return rt.table[fn](obj);
}

// PPAPIInstance constructor(id, rt, info, window, eventHandler, containerWindow, mm)
let instance = new PPAPIInstance(instanceId, rt, info, new Mock_Window(), null /*docShell.chromeEventHandler*/, null, new Mock_MessageManager());

registerCleanupFunction(function () {
  resHandler.setSubstitution("ppapi.js", null);
})

