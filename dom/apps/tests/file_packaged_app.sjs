var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

// From prio.h
const PR_RDWR        = 0x04;
const PR_CREATE_FILE = 0x08;
const PR_TRUNCATE    = 0x20;

Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

var gBasePath = "tests/dom/apps/tests/";
var gMiniManifestTemplate = "file_packaged_app.template.webapp";
var gAppTemplate = "file_packaged_app.template.html";
var gAppName = "appname";
var gDevName = "devname";
var gDevUrl = "http://dev.url";

var gIconData =
"\x89\x50\x4E\x47\x0D\x0A\x1A\x0A\x00\x00\x00\x0D\x49\x48\x44\x52" +
"\x00\x00\x00\x0F\x00\x00\x00\x0F\x08\x03\x00\x00\x00\x0C\x08\x65" +
"\x78\x00\x00\x00\x04\x67\x41\x4D\x41\x00\x00\xD6\xD8\xD4\x4F\x58" +
"\x32\x00\x00\x00\x19\x74\x45\x58\x74\x53\x6F\x66\x74\x77\x61\x72" +
"\x65\x00\x41\x64\x6F\x62\x65\x20\x49\x6D\x61\x67\x65\x52\x65\x61" +
"\x64\x79\x71\xC9\x65\x3C\x00\x00\x00\x39\x50\x4C\x54\x45\xB5\x42" + 
"\x42\xCE\x94\x94\xCE\x84\x84\x9C\x21\x21\xAD\x21\x21\xCE\x73\x73" +
"\x9C\x10\x10\xBD\x52\x52\xEF\xD6\xD6\xB5\x63\x63\xA5\x10\x10\xB5" +
"\x00\x00\xE7\xC6\xC6\xE7\xB5\xB5\x9C\x00\x00\x8C\x00\x00\xFF\xFF" +
"\xFF\x7B\x00\x00\xAD\x00\x00\xEC\xAD\x83\x63\x00\x00\x00\x66\x49" +
"\x44\x41\x54\x78\xDA\x6C\x89\x0B\x12\xC2\x40\x08\x43\xB3\xDB\x8F" +
"\x5A\x21\x40\xEF\x7F\x58\xA1\x55\x67\xB6\xD3\xC0\x03\x42\xF0\xDE" +
"\x87\xC2\x3E\xEA\xCE\xF3\x4B\x0D\x30\x27\xAB\xCF\x03\x9C\xFB\xA3" +
"\xEB\xBC\x2D\xBA\xD4\x0F\x84\x97\x9E\x49\x67\xE5\x74\x87\x26\x70" +
"\x21\x0D\x66\xAE\xB6\x26\xB5\x8D\xE5\xE5\xCF\xB4\x9E\x79\x1C\xB9" +
"\x4C\x2E\xB0\x90\x16\x96\x84\xB6\x68\x2F\x84\x45\xF5\x0F\xC4\xA8" +
"\xAB\xFF\x08\x30\x00\x16\x91\x0C\xDD\xAB\xF1\x05\x11\x00\x00\x00" +
"\x00\x49\x45\x4E\x44\xAE\x42\x60\x82";

function handleRequest(request, response) {
  var query = getQuery(request);

  var packageSize = ("packageSize" in query) ? query.packageSize : 0;
  var appName = ("appName" in query) ? query.appName : gAppName;
  var devName = ("devName" in query) ? query.devName : gDevName;
  var devUrl = ("devUrl" in query) ? query.devUrl : gDevUrl;
  // allowCancel just means deliver the file slowly so we have time to cancel it
  var allowCancel = "allowCancel" in query;
  var getPackage = "getPackage" in query;
  var alreadyDeferred = Number(getState("alreadyDeferred"));
  var role = query.role || "";

  var failPackageDownloadOnce = "failPackageDownloadOnce" in query;
  var failOnce = "failOnce" in query;
  var alreadyFailed = Number(getState("alreadyFailed"));

  if (allowCancel && getPackage && !alreadyDeferred) {
    // Only do this for the actual package delivery.
    response.processAsync();
    // And to avoid timer problems, only do this once.
    setState("alreadyDeferred", "1");
  }

  if (failOnce && !alreadyFailed) {
    setState("alreadyFailed", "1");
    // We need to simulate a network error... let's just try closing the connection
    // without any output
    response.seizePower();
    response.finish();
    return;
  }

  response.setHeader("Access-Control-Allow-Origin", "*", false);

  // If this is a version update, update state, prepare the manifest,
  // the application package and return.
  if ("setVersion" in query) {
    var version = query.setVersion;
    setState("version", version);
    var packageVersion = ("dontUpdatePackage" in query) ? version - 1 : version;
    var packageName = "test_packaged_app_" + packageVersion + ".zip";

    setState("packageName", packageName);
    var packagePath = "/" + gBasePath + "file_packaged_app.sjs?" +
                      (allowCancel ? "allowCancel&" : "") +
                      (failPackageDownloadOnce ? "failOnce&" : "") +
                      "getPackage=" + packageName;
    setState("packagePath", packagePath);

    if (version == packageVersion) {
      // Create the application package.
      var zipWriter = Cc["@mozilla.org/zipwriter;1"]
                        .createInstance(Ci.nsIZipWriter);
      var zipFile = FileUtils.getFile("TmpD", [packageName]);
      zipWriter.open(zipFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);

      // We want to run some tests without the manifest included in the zip.
      if (version != "0") {
        var manifestTemplate = gBasePath + gMiniManifestTemplate;
        var manifest = makeResource(manifestTemplate, version, packagePath,
                                    packageSize, appName, devName, devUrl, role);
        addZipEntry(zipWriter, manifest, "manifest.webapp");
      }

      var appTemplate = gBasePath + gAppTemplate;
      var app = makeResource(appTemplate, version, packagePath, packageSize,
                             appName, devName, devUrl);
      addZipEntry(zipWriter, app, "index.html");

      var iconString = gIconData;
      addZipEntry(zipWriter, iconString, "icon.png");

      zipWriter.close();
    }

    response.setHeader("Content-Type", "text/html", false);
    response.write("OK");
    return;
  }

  // Get the version from server state
  var version = Number(getState("version"));
  var packageName = String(getState("packageName"));
  var packagePath = String(getState("packagePath"));

  var etag = getEtag(request, version);

  if (etagMatches(request, etag)) {
    dump("Etags Match. Sending 304\n");
    response.setStatusLine(request.httpVersion, "304", "Not modified");
    return;
  }

  response.setHeader("Etag", etag, false);

  // Serve the application package corresponding to the requested app version.
  if (getPackage) {
    var resource = readFile(packageName, true);
    response.setHeader("Content-Type",
                       "Content-Type: application/java-archive", false);
    if (allowCancel && !alreadyDeferred) {
      var timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      timer.initWithCallback(function (aTimer) {
        response.write(resource);
        aTimer.cancel();
        response.finish();
      }, 15000, Ci.nsITimer.TYPE_ONE_SHOT);
    } else {
      response.write(resource);
    }
    return;
  }

  // Serve the mini-manifest corresponding to the requested app version.
  if ("getManifest" in query) {
    var template = gBasePath + gMiniManifestTemplate;
    if (!("noManifestContentType" in query)) {
      response.setHeader("Content-Type",
                         "application/x-web-app-manifest+json", false);
    }
    packagePath = "wrongPackagePath" in query ? "" : packagePath;
    var manifest = makeResource(template, version, packagePath, packageSize,
                                appName, devName, devUrl, role);
    response.write(manifest);
    return;
  }

  response.setHeader("Content-type", "text-html", false);
  response.write("KO");
}

function getQuery(request) {
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });
  return query;
}

function getEtag(request, version) {
  return request.queryString.replace(/&/g, '-').replace(/=/g, '-') +
         '-' + version;
}

function etagMatches(request, etag) {
  return request.hasHeader("If-None-Match") &&
         request.getHeader("If-None-Match") == etag;
}

// File and resources helpers

function addZipEntry(zipWriter, entry, entryName) {
  var stream = Cc["@mozilla.org/io/string-input-stream;1"]
               .createInstance(Ci.nsIStringInputStream);
  stream.setData(entry, entry.length);
  zipWriter.addEntryStream(entryName, Date.now(),
                           Ci.nsIZipWriter.COMPRESSION_BEST, stream, false);
}

function readFile(path, fromTmp) {
  var dir = fromTmp ? "TmpD" : "CurWorkD";
  var file = Cc["@mozilla.org/file/directory_service;1"]
             .getService(Ci.nsIProperties)
             .get(dir, Ci.nsILocalFile);
  var fstream = Cc["@mozilla.org/network/file-input-stream;1"]
                .createInstance(Ci.nsIFileInputStream);
  var split = path.split("/");
  for(var i = 0; i < split.length; ++i) {
    file.append(split[i]);
  }
  fstream.init(file, -1, 0, 0);
  var data = NetUtil.readInputStreamToString(fstream, fstream.available());
  fstream.close();
  return data;
}

function makeResource(templatePath, version, packagePath, packageSize,
                      appName, developerName, developerUrl, role) {
  var res = readFile(templatePath, false)
            .replace(/VERSIONTOKEN/g, version)
            .replace(/PACKAGEPATHTOKEN/g, packagePath)
            .replace(/PACKAGESIZETOKEN/g, packageSize)
            .replace(/NAMETOKEN/g, appName)
            .replace(/DEVELOPERTOKEN/g, developerName)
            .replace(/DEVELOPERURLTOKEN/g, developerUrl)
            .replace(/ROLETOKEN/g, role);
  return res;
}
