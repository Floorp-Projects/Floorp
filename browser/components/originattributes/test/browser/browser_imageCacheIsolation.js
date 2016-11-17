/*
 * Bug 1264572 - A test case for image cache isolation.
 */

requestLongerTimeout(2);

let Cu = Components.utils;
let {HttpServer} = Cu.import("resource://testing-common/httpd.js", {});

const NUM_ISOLATION_LOADS = 2;
const NUM_CACHED_LOADS = 1;

let gHits = 0;

let server = new HttpServer();
server.registerPathHandler('/image.png', imageHandler);
server.registerPathHandler('/file.html', fileHandler);
server.start(-1);

registerCleanupFunction(() => {
  server.stop(() => {
    server = null;
  });
});

let BASE_URI = 'http://localhost:' + server.identity.primaryPort;
let IMAGE_URI = BASE_URI + '/image.png';
let FILE_URI = BASE_URI + '/file.html';

function imageHandler(metadata, response) {
  info('XXX: loading image from server');
  gHits++;
  response.setHeader("Cache-Control", "max-age=10000", false);
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "image/png", false);
  var body = "iVBORw0KGgoAAAANSUhEUgAAAAMAAAADCAIAAADZSiLoAAAAEUlEQVQImWP4z8AAQTAamQkAhpcI+DeMzFcAAAAASUVORK5CYII=";
  response.bodyOutputStream.write(body, body.length);
}

function fileHandler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html", false);
  let body = `<html><body><image src=${IMAGE_URI}></body></html>`;
  response.bodyOutputStream.write(body, body.length);
}

function doBefore() {
  // reset hit counter
  info('XXX resetting gHits');
  gHits = 0;
  info('XXX clearing image cache');
  let imageCache = Cc["@mozilla.org/image/tools;1"]
                      .getService(Ci.imgITools)
                      .getImgCacheForDocument(null);
  imageCache.clearCache(true);
  imageCache.clearCache(false);
  info('XXX clearning network cache');
  let networkCache = Cc["@mozilla.org/netwerk/cache-storage-service;1"]
                        .getService(Ci.nsICacheStorageService);
  networkCache.clear();
}

// the test function does nothing on purpose.
function doTest(aBrowser) {
  return 0;
}

// the check function
function doCheck(shouldIsolate, a, b) {
  // if we're doing first party isolation and the image cache isolation is
  // working, then gHits should be 2 because the image would have been loaded
  // one per first party domain.  if first party isolation is disabled, then
  // gHits should be 1 since there would be one image load from the server and
  // one load from the image cache.
  info(`XXX check: gHits == ${gHits}, shouldIsolate == ${shouldIsolate}`);
  return shouldIsolate ? gHits == NUM_ISOLATION_LOADS
                       : gHits == NUM_CACHED_LOADS;
}

IsolationTestTools.runTests(FILE_URI, doTest, doCheck, doBefore);
