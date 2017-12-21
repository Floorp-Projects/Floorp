/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

/**
 * This file tests the DirectoryLinksProvider singleton in the DirectoryLinksProvider.jsm module.
 */

var { classes: Cc, interfaces: Ci, results: Cr, utils: Cu, Constructor: CC } = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/DirectoryLinksProvider.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Http.jsm");
Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NewTabUtils",
  "resource://gre/modules/NewTabUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PlacesTestUtils",
  "resource://testing-common/PlacesTestUtils.jsm");

do_get_profile();

const DIRECTORY_LINKS_FILE = "directoryLinks.json";
const DIRECTORY_FRECENCY = 1000;
const kURLData = {"directory": [{"url": "http://example.com", "title": "LocalSource"}]};
const kTestURL = "data:application/json," + JSON.stringify(kURLData);

// DirectoryLinksProvider preferences
const kSourceUrlPref = DirectoryLinksProvider._observedPrefs.linksURL;
const kNewtabEnhancedPref = "browser.newtabpage.enhanced";

// httpd settings
var server;
const kDefaultServerPort = 9000;
const kBaseUrl = "http://localhost:" + kDefaultServerPort;
const kExamplePath = "/exampleTest/";
const kFailPath = "/fail/";
const kExampleURL = kBaseUrl + kExamplePath;
const kFailURL = kBaseUrl + kFailPath;

// app/profile/firefox.js are not avaialble in xpcshell: hence, preset them
const origReqLocales = Services.locale.getRequestedLocales();
Services.locale.setRequestedLocales(["en-US"]);
Services.prefs.setCharPref(kSourceUrlPref, kTestURL);
Services.prefs.setBoolPref(kNewtabEnhancedPref, true);

const kHttpHandlerData = {};
kHttpHandlerData[kExamplePath] = {"directory": [{"url": "http://example.com", "title": "RemoteSource"}]};

const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                              "nsIBinaryInputStream",
                              "setInputStream");

var gLastRequestPath;

function getHttpHandler(path) {
  let code = 200;
  let body = JSON.stringify(kHttpHandlerData[path]);
  if (path == kFailPath) {
    code = 204;
  }
  return function(aRequest, aResponse) {
    gLastRequestPath = aRequest.path;
    aResponse.setStatusLine(null, code);
    aResponse.setHeader("Content-Type", "application/json");
    aResponse.write(body);
  };
}

function isIdentical(actual, expected) {
  if (expected == null) {
    Assert.equal(actual, expected);
  } else if (typeof expected == "object") {
    // Make sure all the keys match up
    Assert.equal(Object.keys(actual).sort() + "", Object.keys(expected).sort());

    // Recursively check each value individually
    Object.keys(expected).forEach(key => {
      isIdentical(actual[key], expected[key]);
    });
  } else {
    Assert.equal(actual, expected);
  }
}

function fetchData() {
  return new Promise(resolve => {

    DirectoryLinksProvider.getLinks(linkData => {
      resolve(linkData);
    });
  });
}

function readJsonFile(jsonFile = DIRECTORY_LINKS_FILE) {
  let decoder = new TextDecoder();
  let directoryLinksFilePath = OS.Path.join(OS.Constants.Path.localProfileDir, jsonFile);
  return OS.File.read(directoryLinksFilePath).then(array => {
    let json = decoder.decode(array);
    return JSON.parse(json);
  }, () => { return ""; });
}

function cleanJsonFile(jsonFile = DIRECTORY_LINKS_FILE) {
  let directoryLinksFilePath = OS.Path.join(OS.Constants.Path.localProfileDir, jsonFile);
  return OS.File.remove(directoryLinksFilePath);
}

function LinksChangeObserver() {
  this.deferred = Promise.defer();
  this.onManyLinksChanged = () => this.deferred.resolve();
  this.onDownloadFail = this.onManyLinksChanged;
}

function promiseDirectoryDownloadOnPrefChange(pref, newValue) {
  let oldValue = Services.prefs.getCharPref(pref);
  if (oldValue != newValue) {
    // if the preference value is already equal to newValue
    // the pref service will not call our observer and we
    // deadlock. Hence only setup observer if values differ
    let observer = new LinksChangeObserver();
    DirectoryLinksProvider.addObserver(observer);
    Services.prefs.setCharPref(pref, newValue);
    return observer.deferred.promise.then(() => {
      DirectoryLinksProvider.removeObserver(observer);
    });
  }
  return Promise.resolve();
}

function promiseSetupDirectoryLinksProvider(options = {}) {
  return (async function() {
    let linksURL = options.linksURL || kTestURL;
    await DirectoryLinksProvider.init();
    Services.locale.setRequestedLocales([options.locale || "en-US"]);
    await promiseDirectoryDownloadOnPrefChange(kSourceUrlPref, linksURL);
    Assert.equal(DirectoryLinksProvider._linksURL, linksURL);
    DirectoryLinksProvider._lastDownloadMS = options.lastDownloadMS || 0;
  })();
}

function promiseCleanDirectoryLinksProvider() {
  return (async function() {
    Services.locale.setRequestedLocales(["en-US"]);
    await promiseDirectoryDownloadOnPrefChange(kSourceUrlPref, kTestURL);
    DirectoryLinksProvider._lastDownloadMS  = 0;
    DirectoryLinksProvider.reset();
  })();
}

function run_test() {
  // Set up a mock HTTP server to serve a directory page
  server = new HttpServer();
  server.registerPrefixHandler(kExamplePath, getHttpHandler(kExamplePath));
  server.registerPrefixHandler(kFailPath, getHttpHandler(kFailPath));
  server.start(kDefaultServerPort);
  NewTabUtils.init();

  run_next_test();

  // Teardown.
  do_register_cleanup(function() {
    server.stop(function() { });
    DirectoryLinksProvider.reset();
    Services.locale.setRequestedLocales(origReqLocales);
    Services.prefs.clearUserPref(kSourceUrlPref);
    Services.prefs.clearUserPref(kNewtabEnhancedPref);
  });
}


function setTimeout(fun, timeout) {
  let timer = Components.classes["@mozilla.org/timer;1"]
                        .createInstance(Components.interfaces.nsITimer);
  var event = {
    notify() {
      fun();
    }
  };
  timer.initWithCallback(event, timeout,
                         Components.interfaces.nsITimer.TYPE_ONE_SHOT);
  return timer;
}

add_task(async function test_fetchAndCacheLinks_local() {
  await DirectoryLinksProvider.init();
  await cleanJsonFile();
  // Trigger cache of data or chrome uri files in profD
  await DirectoryLinksProvider._fetchAndCacheLinks(kTestURL);
  let data = await readJsonFile();
  isIdentical(data, kURLData);
});

add_task(async function test_fetchAndCacheLinks_remote() {
  await DirectoryLinksProvider.init();
  await cleanJsonFile();
  // this must trigger directory links json download and save it to cache file
  await promiseDirectoryDownloadOnPrefChange(kSourceUrlPref, kExampleURL + "%LOCALE%");
  Assert.equal(gLastRequestPath, kExamplePath + "en-US");
  let data = await readJsonFile();
  isIdentical(data, kHttpHandlerData[kExamplePath]);
});

add_task(async function test_fetchAndCacheLinks_malformedURI() {
  await DirectoryLinksProvider.init();
  await cleanJsonFile();
  let someJunk = "some junk";
  try {
    await DirectoryLinksProvider._fetchAndCacheLinks(someJunk);
    do_throw("Malformed URIs should fail");
  } catch (e) {
    Assert.equal(e, "Error fetching " + someJunk);
  }

  // File should be empty.
  let data = await readJsonFile();
  isIdentical(data, "");
});

add_task(async function test_fetchAndCacheLinks_unknownHost() {
  await DirectoryLinksProvider.init();
  await cleanJsonFile();
  let nonExistentServer = "http://localhost:56789/";
  try {
    await DirectoryLinksProvider._fetchAndCacheLinks(nonExistentServer);
    do_throw("BAD URIs should fail");
  } catch (e) {
    Assert.ok(e.startsWith("Fetching " + nonExistentServer + " results in error code: "));
  }

  // File should be empty.
  let data = await readJsonFile();
  isIdentical(data, "");
});

add_task(async function test_fetchAndCacheLinks_non200Status() {
  await DirectoryLinksProvider.init();
  await cleanJsonFile();
  await promiseDirectoryDownloadOnPrefChange(kSourceUrlPref, kFailURL);
  Assert.equal(gLastRequestPath, kFailPath);
  let data = await readJsonFile();
  isIdentical(data, {});
});

// To test onManyLinksChanged observer, trigger a fetch
add_task(async function test_DirectoryLinksProvider__linkObservers() {
  await DirectoryLinksProvider.init();

  let testObserver = new LinksChangeObserver();
  DirectoryLinksProvider.addObserver(testObserver);
  Assert.equal(DirectoryLinksProvider._observers.size, 1);
  DirectoryLinksProvider._fetchAndCacheLinksIfNecessary(true);

  await testObserver.deferred.promise;
  DirectoryLinksProvider._removeObservers();
  Assert.equal(DirectoryLinksProvider._observers.size, 0);

  await promiseCleanDirectoryLinksProvider();
});

add_task(async function test_DirectoryLinksProvider__prefObserver_url() {
  await promiseSetupDirectoryLinksProvider({linksURL: kTestURL});

  let links = await fetchData();
  Assert.equal(links.length, 1);
  let expectedData =  [{url: "http://example.com", title: "LocalSource", frecency: DIRECTORY_FRECENCY, lastVisitDate: 1}];
  isIdentical(links, expectedData);

  // tests these 2 things:
  // 1. _linksURL is properly set after the pref change
  // 2. invalid source url is correctly handled
  let exampleUrl = "http://localhost:56789/bad";
  await promiseDirectoryDownloadOnPrefChange(kSourceUrlPref, exampleUrl);
  Assert.equal(DirectoryLinksProvider._linksURL, exampleUrl);

  // since the download fail, the directory file must remain the same
  let newLinks = await fetchData();
  isIdentical(newLinks, expectedData);

  // now remove the file, and re-download
  await cleanJsonFile();
  await promiseDirectoryDownloadOnPrefChange(kSourceUrlPref, exampleUrl + " ");
  // we now should see empty links
  newLinks = await fetchData();
  isIdentical(newLinks, []);

  await promiseCleanDirectoryLinksProvider();
});

add_task(async function test_DirectoryLinksProvider_getLinks_noDirectoryData() {
  let data = {
    "directory": [],
  };
  let dataURI = "data:application/json," + JSON.stringify(data);
  await promiseSetupDirectoryLinksProvider({linksURL: dataURI});

  let links = await fetchData();
  Assert.equal(links.length, 0);
  await promiseCleanDirectoryLinksProvider();
});

add_task(async function test_DirectoryLinksProvider_getLinks_badData() {
  let data = {
    "en-US": {
      "en-US": [{url: "http://example.com", title: "US"}],
    },
  };
  let dataURI = "data:application/json," + JSON.stringify(data);
  await promiseSetupDirectoryLinksProvider({linksURL: dataURI});

  // Make sure we get nothing for incorrectly formatted data
  let links = await fetchData();
  Assert.equal(links.length, 0);
  await promiseCleanDirectoryLinksProvider();
});

add_task(function test_DirectoryLinksProvider_needsDownload() {
  // test timestamping
  DirectoryLinksProvider._lastDownloadMS = 0;
  Assert.ok(DirectoryLinksProvider._needsDownload);
  DirectoryLinksProvider._lastDownloadMS = Date.now();
  Assert.ok(!DirectoryLinksProvider._needsDownload);
  DirectoryLinksProvider._lastDownloadMS = Date.now() - (60 * 60 * 24 + 1) * 1000;
  Assert.ok(DirectoryLinksProvider._needsDownload);
  DirectoryLinksProvider._lastDownloadMS = 0;
});

add_task(async function test_DirectoryLinksProvider_fetchAndCacheLinksIfNecessary() {
  await DirectoryLinksProvider.init();
  await cleanJsonFile();
  // explicitly change source url to cause the download during setup
  await promiseSetupDirectoryLinksProvider({linksURL: kTestURL + " "});
  await DirectoryLinksProvider._fetchAndCacheLinksIfNecessary();

  // inspect lastDownloadMS timestamp which should be 5 seconds less then now()
  let lastDownloadMS = DirectoryLinksProvider._lastDownloadMS;
  Assert.ok((Date.now() - lastDownloadMS) < 5000);

  // we should have fetched a new file during setup
  let data = await readJsonFile();
  isIdentical(data, kURLData);

  // attempt to download again - the timestamp should not change
  await DirectoryLinksProvider._fetchAndCacheLinksIfNecessary();
  Assert.equal(DirectoryLinksProvider._lastDownloadMS, lastDownloadMS);

  // clean the file and force the download
  await cleanJsonFile();
  await DirectoryLinksProvider._fetchAndCacheLinksIfNecessary(true);
  data = await readJsonFile();
  isIdentical(data, kURLData);

  // make sure that failed download does not corrupt the file, nor changes lastDownloadMS
  lastDownloadMS = DirectoryLinksProvider._lastDownloadMS;
  await promiseDirectoryDownloadOnPrefChange(kSourceUrlPref, "http://");
  await DirectoryLinksProvider._fetchAndCacheLinksIfNecessary(true);
  data = await readJsonFile();
  isIdentical(data, kURLData);
  Assert.equal(DirectoryLinksProvider._lastDownloadMS, lastDownloadMS);

  // _fetchAndCacheLinksIfNecessary must return same promise if download is in progress
  let downloadPromise = DirectoryLinksProvider._fetchAndCacheLinksIfNecessary(true);
  let anotherPromise = DirectoryLinksProvider._fetchAndCacheLinksIfNecessary(true);
  Assert.ok(downloadPromise === anotherPromise);
  await downloadPromise;

  await promiseCleanDirectoryLinksProvider();
});

add_task(async function test_DirectoryLinksProvider_fetchDirectoryOnPrefChange() {
  await DirectoryLinksProvider.init();

  let testObserver = new LinksChangeObserver();
  DirectoryLinksProvider.addObserver(testObserver);

  await cleanJsonFile();
  // ensure that provider does not think it needs to download
  Assert.ok(!DirectoryLinksProvider._needsDownload);

  // change the source URL, which should force directory download
  await promiseDirectoryDownloadOnPrefChange(kSourceUrlPref, kExampleURL);
  // then wait for testObserver to fire and test that json is downloaded
  await testObserver.deferred.promise;
  Assert.equal(gLastRequestPath, kExamplePath);
  let data = await readJsonFile();
  isIdentical(data, kHttpHandlerData[kExamplePath]);

  await promiseCleanDirectoryLinksProvider();
});

add_task(async function test_DirectoryLinksProvider_fetchDirectoryOnInit() {
  // ensure preferences are set to defaults
  await promiseSetupDirectoryLinksProvider();
  // now clean to provider, so we can init it again
  await promiseCleanDirectoryLinksProvider();

  await cleanJsonFile();
  await DirectoryLinksProvider.init();
  let data = await readJsonFile();
  isIdentical(data, kURLData);

  await promiseCleanDirectoryLinksProvider();
});

add_task(async function test_DirectoryLinksProvider_getLinksFromCorruptedFile() {
  await promiseSetupDirectoryLinksProvider();

  // write bogus json to a file and attempt to fetch from it
  let directoryLinksFilePath = OS.Path.join(OS.Constants.Path.profileDir, DIRECTORY_LINKS_FILE);
  await OS.File.writeAtomic(directoryLinksFilePath, '{"en-US":');
  let data = await fetchData();
  isIdentical(data, []);

  await promiseCleanDirectoryLinksProvider();
});

add_task(async function test_DirectoryLinksProvider_getAllowedLinks() {
  let data = {"directory": [
    {url: "ftp://example.com"},
    {url: "http://example.net"},
    {url: "javascript:5"},
    {url: "https://example.com"},
    {url: "httpJUNKjavascript:42"},
    {url: "data:text/plain,hi"},
    {url: "http/bork:eh"},
  ]};
  let dataURI = "data:application/json," + JSON.stringify(data);
  await promiseSetupDirectoryLinksProvider({linksURL: dataURI});

  let links = await fetchData();
  Assert.equal(links.length, 2);

  // The only remaining url should be http and https
  Assert.equal(links[0].url, data.directory[1].url);
  Assert.equal(links[1].url, data.directory[3].url);
});

add_task(async function test_DirectoryLinksProvider_getAllowedImages() {
  let data = {"directory": [
    {url: "http://example.com", imageURI: "ftp://example.com"},
    {url: "http://example.com", imageURI: "http://example.net"},
    {url: "http://example.com", imageURI: "javascript:5"},
    {url: "http://example.com", imageURI: "https://example.com"},
    {url: "http://example.com", imageURI: "httpJUNKjavascript:42"},
    {url: "http://example.com", imageURI: "data:text/plain,hi"},
    {url: "http://example.com", imageURI: "http/bork:eh"},
  ]};
  let dataURI = "data:application/json," + JSON.stringify(data);
  await promiseSetupDirectoryLinksProvider({linksURL: dataURI});

  let links = await fetchData();
  Assert.equal(links.length, 2);

  // The only remaining images should be https and data
  Assert.equal(links[0].imageURI, data.directory[3].imageURI);
  Assert.equal(links[1].imageURI, data.directory[5].imageURI);
});

add_task(async function test_DirectoryLinksProvider_getAllowedImages_base() {
  let data = {"directory": [
    {url: "http://example1.com", imageURI: "https://example.com"},
    {url: "http://example2.com", imageURI: "https://tiles.cdn.mozilla.net"},
    {url: "http://example3.com", imageURI: "https://tiles2.cdn.mozilla.net"},
    {url: "http://example4.com", enhancedImageURI: "https://mozilla.net"},
    {url: "http://example5.com", imageURI: "data:text/plain,hi"},
  ]};
  let dataURI = "data:application/json," + JSON.stringify(data);
  await promiseSetupDirectoryLinksProvider({linksURL: dataURI});

  // Pretend we're using the default pref to trigger base matching
  DirectoryLinksProvider.__linksURLModified = false;

  let links = await fetchData();
  Assert.equal(links.length, 4);

  // The only remaining images should be https with mozilla.net or data URI
  Assert.equal(links[0].url, data.directory[1].url);
  Assert.equal(links[1].url, data.directory[2].url);
  Assert.equal(links[2].url, data.directory[3].url);
  Assert.equal(links[3].url, data.directory[4].url);
});

add_task(async function test_DirectoryLinksProvider_getAllowedEnhancedImages() {
  let data = {"directory": [
    {url: "http://example.com", enhancedImageURI: "ftp://example.com"},
    {url: "http://example.com", enhancedImageURI: "http://example.net"},
    {url: "http://example.com", enhancedImageURI: "javascript:5"},
    {url: "http://example.com", enhancedImageURI: "https://example.com"},
    {url: "http://example.com", enhancedImageURI: "httpJUNKjavascript:42"},
    {url: "http://example.com", enhancedImageURI: "data:text/plain,hi"},
    {url: "http://example.com", enhancedImageURI: "http/bork:eh"},
  ]};
  let dataURI = "data:application/json," + JSON.stringify(data);
  await promiseSetupDirectoryLinksProvider({linksURL: dataURI});

  let links = await fetchData();
  Assert.equal(links.length, 2);

  // The only remaining enhancedImages should be http and https and data
  Assert.equal(links[0].enhancedImageURI, data.directory[3].enhancedImageURI);
  Assert.equal(links[1].enhancedImageURI, data.directory[5].enhancedImageURI);
});

add_task(function test_DirectoryLinksProvider_setDefaultEnhanced() {
  function checkDefault(expected) {
    Services.prefs.clearUserPref(kNewtabEnhancedPref);
    Assert.equal(Services.prefs.getBoolPref(kNewtabEnhancedPref), expected);
  }

  // Use the default donottrack prefs (enabled = false)
  Services.prefs.clearUserPref("privacy.donottrackheader.enabled");
  checkDefault(true);

  // Turn on DNT - no track
  Services.prefs.setBoolPref("privacy.donottrackheader.enabled", true);
  checkDefault(false);

  // Turn off DNT header
  Services.prefs.clearUserPref("privacy.donottrackheader.enabled");
  checkDefault(true);

  // Clean up
  Services.prefs.clearUserPref("privacy.donottrackheader.value");
});

add_task(function test_DirectoryLinksProvider_anonymous() {
  Assert.ok(DirectoryLinksProvider._newXHR().mozAnon);
});
