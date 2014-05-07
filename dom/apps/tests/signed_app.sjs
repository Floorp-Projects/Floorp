var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

// From prio.h
const PR_RDWR        = 0x04;
const PR_CREATE_FILE = 0x08;
const PR_TRUNCATE    = 0x20;

const CUR_WORK_DIR = "CurWorkD";

Cu.import("resource://gre/modules/NetUtil.jsm");

var gBasePath = "tests/dom/apps/tests/";
var gMiniManifestTemplate = "signed_app_template.webapp";
var gAppName = "Simple App";
var gDevName = "David Clarke";
var gDevUrl = "http://dev.url";

function handleRequest(request, response) {
  var query = getQuery(request);

  response.setHeader("Access-Control-Allow-Origin", "*", false);

  var version = ("version" in query) ? query.version : "1";
  var app = ("app" in query) ? query.app : "valid";
  var prevVersion = getState("version");

  if (version != prevVersion) {
    setState("version", version);
  }
  var packageName = app + "_app_" + version + ".zip";
  setState("packageName", packageName);
  var packagePath = "/" + gBasePath + "signed/" + packageName;
  setState("packagePath", packagePath);
  var packageSize = readSize(packagePath);

  var etag = getEtag(request, version);

  if (etagMatches(request, etag)) {
    dump("Etags Match. Sending 304\n");
    response.setStatusLine(request.httpVersion, "304", "Not modified");
    return;
  }
  response.setHeader("Etag", etag, false);

  // Serve the mini-manifest corresponding to the requested app version.
  var template = gBasePath + gMiniManifestTemplate;

  response.setHeader("Content-Type",
                     "application/x-web-app-manifest+json", false);
  var manifest = makeResource(template, version, packagePath, packageSize,
                              gAppName, gDevName, gDevUrl);
  response.write(manifest);
}

function getQuery(request) {
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[decodeURIComponent(name)] = decodeURIComponent(value);
  });
  return query;
}

function getEtag(request, version) {
  return request.queryString.replace(/[&=]/g, '-') + '-' + version;
}

function etagMatches(request, etag) {
  return request.hasHeader("If-None-Match") &&
         request.getHeader("If-None-Match") == etag;
}

// File and resources helpers

function readSize(path) {
  var file = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIProperties).get(CUR_WORK_DIR, Ci.nsILocalFile);
  var split = path.split("/");
  for (var i = 0; i < split.length; ++i) {
    file.append(split[i]);
  }
  return file.fileSize;
}

function readFile(path) {
  var file = Cc["@mozilla.org/file/directory_service;1"]
             .getService(Ci.nsIProperties)
             .get(CUR_WORK_DIR, Ci.nsILocalFile);
  var fstream = Cc["@mozilla.org/network/file-input-stream;1"]
                .createInstance(Ci.nsIFileInputStream);
  var split = path.split("/");
  for (var i = 0; i < split.length; ++i) {
    file.append(split[i]);
  }
  fstream.init(file, -1, 0, 0);
  var data = NetUtil.readInputStreamToString(fstream, fstream.available());
  fstream.close();
  return data;
}

function makeResource(templatePath, version, packagePath, packageSize,
                      appName, developerName, developerUrl) {
  var res = readFile(templatePath).
              replace(/VERSIONTOKEN/g, version).
              replace(/PACKAGEPATHTOKEN/g, packagePath).
              replace(/PACKAGESIZETOKEN/g, packageSize).
              replace(/NAMETOKEN/g, appName);
  return res;
}
