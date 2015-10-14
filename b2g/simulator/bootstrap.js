var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

// Useful piece of code from :bent
// http://mxr.mozilla.org/mozilla-central/source/dom/workers/test/extensions/bootstrap/bootstrap.js
function registerAddonResourceHandler(data) {
  let file = data.installPath;
  let fileuri = file.isDirectory() ?
                Services.io.newFileURI(file) :
                Services.io.newURI("jar:" + file.path + "!/", null, null);
  let resourceName = encodeURIComponent(data.id.replace("@", "at"));

  Services.io.getProtocolHandler("resource").
              QueryInterface(Ci.nsIResProtocolHandler).
              setSubstitution(resourceName, fileuri);

  return "resource://" + resourceName + "/";
}

var mainModule;

function install(data, reason) {}
function uninstall(data, reason) {}

function startup(data, reason) {
  let uri = registerAddonResourceHandler(data);

  let loaderModule =
    Cu.import('resource://gre/modules/commonjs/toolkit/loader.js').Loader;
  let { Loader, Require, Main } = loaderModule;

  const { ConsoleAPI } = Cu.import("resource://gre/modules/Console.jsm");

  let loader = Loader({
    paths: {
      "./": uri + "lib/",
      "": "resource://gre/modules/commonjs/"
    },
    globals: {
      console: new ConsoleAPI({
        prefix: data.id
      })
    },
    modules: {
      "toolkit/loader": loaderModule,
      addon: {
        id: data.id,
        version: data.version,
        uri: uri
      }
    }
  });

  let require_ = Require(loader, { id: "./addon" });
  mainModule = require_("./main");
}

function shutdown(data, reason) {
  if (mainModule && mainModule.shutdown) {
    mainModule.shutdown();
  }
}

function uninstall(data, reason) {}
