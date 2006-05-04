const Cc = Components.classes;
const Ci = Components.interfaces;
const G_GDEBUG = true;

// Use subscript loader to load files.  The files in ../content get mapped
// to chrome://browser/content/safebrowsing/.  Order matters if one file depends
// on another file during initialization.
const LIB_FILES = [
  // some files are from toolkit
  "chrome://global/content/url-classifier/js/lang.js",
  "chrome://global/content/url-classifier/moz/preferences.js",
  "chrome://global/content/url-classifier/moz/filesystem.js",
  "chrome://global/content/url-classifier/moz/debug.js", // req js/lang.js moz/prefs.js moz/filesystem.js

  "chrome://global/content/url-classifier/js/arc4.js",
  "chrome://global/content/url-classifier/moz/alarm.js",
  "chrome://global/content/url-classifier/moz/base64.js",
  "chrome://global/content/url-classifier/moz/cryptohasher.js",
  "chrome://global/content/url-classifier/moz/lang.js",
  "chrome://global/content/url-classifier/moz/objectsafemap.js",
  "chrome://global/content/url-classifier/moz/observer.js",
  "chrome://global/content/url-classifier/moz/protocol4.js",
  "chrome://global/content/url-classifier/application.js",
  "chrome://global/content/url-classifier/url-crypto.js",
  "chrome://global/content/url-classifier/url-crypto-key-manager.js",
  "chrome://global/content/url-classifier/xml-fetcher.js",

  // some are in browser
  "chrome://browser/content/safebrowsing/js/eventregistrar.js",
  "chrome://browser/content/safebrowsing/js/listdictionary.js",
  "chrome://browser/content/safebrowsing/moz/navwatcher.js",
  "chrome://browser/content/safebrowsing/moz/tabbedbrowserwatcher.js",

  "chrome://browser/content/safebrowsing/application.js",
  "chrome://browser/content/safebrowsing/browser-view.js",
  "chrome://browser/content/safebrowsing/controller.js",
  "chrome://browser/content/safebrowsing/firefox-commands.js",
  "chrome://browser/content/safebrowsing/globalstore.js",
  "chrome://browser/content/safebrowsing/list-warden.js",
  "chrome://browser/content/safebrowsing/phishing-afterload-displayer.js",
  "chrome://browser/content/safebrowsing/phishing-warden.js",
  "chrome://browser/content/safebrowsing/reporter.js",
  "chrome://browser/content/safebrowsing/tr-fetcher.js",
];

for (var i = 0, libFile; libFile = LIB_FILES[i]; ++i) {
  //dump('*** loading subscript ' + libFile + '\n');
  Cc["@mozilla.org/moz/jssubscript-loader;1"]
    .getService(Ci.mozIJSSubScriptLoader)
    .loadSubScript(libFile);
}

// Module object
function SafebrowsingApplicationMod() {
  this.firstTime = true;
  this.cid = Components.ID("{c64d0bcb-8270-4ca7-a0b3-3380c8ffecb5}");
  this.progid = "@mozilla.org/safebrowsing/application;1";
}

SafebrowsingApplicationMod.prototype.registerSelf = function(compMgr, fileSpec, loc, type) {
  if (this.firstTime) {
    this.firstTime = false;
    throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
  }
  compMgr = compMgr.QueryInterface(Ci.nsIComponentRegistrar);
  compMgr.registerFactoryLocation(this.cid,
                                  "Safebrowsing Application Module",
                                  this.progid,
                                  fileSpec,
                                  loc,
                                  type);
};

SafebrowsingApplicationMod.prototype.getClassObject = function(compMgr, cid, iid) {  
  if (!cid.equals(this.cid))
    throw Components.results.NS_ERROR_NO_INTERFACE;
  if (!iid.equals(Ci.nsIFactory))
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

  return this.factory;
}

SafebrowsingApplicationMod.prototype.canUnload = function(compMgr) {
  return true;
}

SafebrowsingApplicationMod.prototype.factory = {
  createInstance: function(outer, iid) {
    if (outer != null)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return new PROT_Application();
  }
};

var ApplicationModInst = new SafebrowsingApplicationMod();

function NSGetModule(compMgr, fileSpec) {
  return ApplicationModInst;
}
