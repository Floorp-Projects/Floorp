/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;
var CC = Components.Constructor;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");
const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const Services = require("Services");
const {DebuggerClient} = require("devtools/shared/client/main");
const {DebuggerServer} = require("devtools/server/main");
const {AppActorFront} = require("devtools/shared/apps/app-actor-front");

var gClient, gActor, gActorFront;

function connect(onDone) {
  // Initialize a loopback remote protocol connection
  DebuggerServer.init();
  // We need to register browser actors to have `listTabs` working
  // and also have a root actor
  DebuggerServer.addBrowserActors();

  // Setup client and actor used in all tests
  gClient = new DebuggerClient(DebuggerServer.connectPipe());
  gClient.connect()
    .then(() => gClient.listTabs())
    .then(aResponse => {
      gActor = aResponse.webappsActor;
      gActorFront = new AppActorFront(gClient, aResponse);
      onDone();
    });
}

function webappActorRequest(request, onResponse) {
  if (!gActor) {
    connect(webappActorRequest.bind(null, request, onResponse));
    return;
  }

  request.to = gActor;
  gClient.request(request, onResponse);
}

// Install a test packaged webapp from data folder
function installTestApp(zipName, appId, onDone) {
  // Copy our package to tmp folder, where the actor retrieves it
  let zip = do_get_file("data/" + zipName);
  let appDir = FileUtils.getDir("TmpD", ["b2g", appId], true, true);
  zip.copyTo(appDir, "application.zip");

  let request = {type: "install", appId: appId};
  webappActorRequest(request, function (aResponse) {
    do_check_eq(aResponse.appId, appId);
    if ("error" in aResponse) {
      do_throw("Error: " + aResponse.error);
    }
    if ("message" in aResponse) {
      do_throw("Error message: " + aResponse.message);
    }
    do_check_false("error" in aResponse);

    onDone();
  });
}

function setup() {
  // We have to setup a profile, otherwise indexed db used by webapps
  // will throw random exception when trying to get profile folder
  do_get_profile();

  // The webapps dir isn't registered on b2g xpcshell tests,
  // we have to manually set it to the directory service.
  do_get_webappsdir();

  // We also need a valid nsIXulAppInfo service as Webapps.jsm is querying it
  Components.utils.import("resource://testing-common/AppInfo.jsm");
  updateAppInfo();

  Components.utils.import("resource://gre/modules/Webapps.jsm");

  // Enable launch/close method of the webapps actor
  let {WebappsActor} = require("devtools/server/actors/webapps");
  WebappsActor.prototype.supportsLaunch = true;
}

function do_get_webappsdir() {
  var webappsDir = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
  webappsDir.append("test_webapps");
  if (!webappsDir.exists())
    webappsDir.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("755", 8));

  var coreAppsDir = Services.dirsvc.get("ProfD", Ci.nsILocalFile);
  coreAppsDir.append("test_coreapps");
  if (!coreAppsDir.exists())
    coreAppsDir.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("755", 8));

  // Register our own provider for the profile directory.
  // It will return our special docshell profile directory.
  var provider = {
    getFile: function (prop, persistent) {
      persistent.value = true;
      if (prop == "webappsDir") {
        return webappsDir.clone();
      }
      else if (prop == "coreAppsDir") {
        return coreAppsDir.clone();
      }
      throw Cr.NS_ERROR_FAILURE;
    },
    QueryInterface: function (iid) {
      if (iid.equals(Ci.nsIDirectoryServiceProvider) ||
          iid.equals(Ci.nsISupports)) {
        return this;
      }
      throw Cr.NS_ERROR_NO_INTERFACE;
    }
  };
  Services.dirsvc.QueryInterface(Ci.nsIDirectoryService).registerProvider(provider);
}


