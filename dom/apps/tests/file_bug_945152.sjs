const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

// From prio.h
const PR_RDWR        = 0x04;
const PR_CREATE_FILE = 0x08;
const PR_TRUNCATE    = 0x20;

Cu.import("resource://gre/modules/FileUtils.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

var gBasePath = "chrome/dom/apps/tests/";
var gAppPath = gBasePath + "file_bug_945152.html";
var gData1 = "TEST_DATA_1:ABCDEFGHIJKLMNOPQRSTUVWXYZ";
var gData2 = "TEST_DATA_2:1234567890";
var gPaddingChar = '.';
var gPaddingSize = 10000;
var gPackageName = "file_bug_945152.zip";
var gManifestData = {
  name: "Testing app",
  description: "Testing app",
  package_path: "http://test/chrome/dom/apps/tests/file_bug_945152.sjs?getPackage=1",
  launch_path: "/index.html",
  developer: {
    name: "devname",
    url: "http://dev.url"
  },
  default_locale: "en-US"
};

function handleRequest(request, response) {
  var query = getQuery(request);

  // Create the packaged app.
  if ("createApp" in query) {
    var zipWriter = Cc["@mozilla.org/zipwriter;1"]
                      .createInstance(Ci.nsIZipWriter);
    var zipFile = FileUtils.getFile("TmpD", [gPackageName]);
    zipWriter.open(zipFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);

    var manifest = JSON.stringify(gManifestData);
    addZipEntry(zipWriter, manifest, "manifest.webapp", Ci.nsIZipWriter.COMPRESSION_BEST);
    var app = readFile(gAppPath, false);
    addZipEntry(zipWriter, app, "index.html", Ci.nsIZipWriter.COMPRESSION_BEST);
    var padding = "";
    for (var i = 0; i < gPaddingSize; i++) {
      padding += gPaddingChar;
    }
    var data = gData1 + padding;
    addZipEntry(zipWriter, data, "data_1.txt", Ci.nsIZipWriter.COMPRESSION_NONE);
    data = gData2 + padding;
    addZipEntry(zipWriter, data, "data_2.txt", Ci.nsIZipWriter.COMPRESSION_BEST);

    zipWriter.alignStoredFiles(4096);
    zipWriter.close();

    response.setHeader("Content-Type", "text/html", false);
    response.write("OK");
    return;
  }

  // Check if we're generating a webapp manifest.
  if ("getManifest" in query) {
    response.setHeader("Content-Type", "application/x-web-app-manifest+json", false);
    response.write(JSON.stringify(gManifestData));
    return;
  }

  // Serve the application package.
  if ("getPackage" in query) {
    var resource = readFile(gPackageName, true);
    response.setHeader("Content-Type",
                       "Content-Type: application/java-archive", false);
    response.write(resource);
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

// File and resources helpers

function addZipEntry(zipWriter, entry, entryName, compression) {
  var stream = Cc["@mozilla.org/io/string-input-stream;1"]
               .createInstance(Ci.nsIStringInputStream);
  stream.setData(entry, entry.length);
  zipWriter.addEntryStream(entryName, Date.now(),
                           compression, stream, false);
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
