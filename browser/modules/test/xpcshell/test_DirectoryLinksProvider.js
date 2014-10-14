/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

/**
 * This file tests the DirectoryLinksProvider singleton in the DirectoryLinksProvider.jsm module.
 */

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu, Constructor: CC } = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/DirectoryLinksProvider.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Http.jsm");
Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/osfile.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
  "resource://gre/modules/NetUtil.jsm");

do_get_profile();

const DIRECTORY_LINKS_FILE = "directoryLinks.json";
const DIRECTORY_FRECENCY = 1000;
const kURLData = {"en-US": [{"url":"http://example.com","title":"LocalSource"}]};
const kTestURL = 'data:application/json,' + JSON.stringify(kURLData);

// DirectoryLinksProvider preferences
const kLocalePref = DirectoryLinksProvider._observedPrefs.prefSelectedLocale;
const kSourceUrlPref = DirectoryLinksProvider._observedPrefs.linksURL;
const kPingUrlPref = "browser.newtabpage.directory.ping";
const kNewtabEnhancedPref = "browser.newtabpage.enhanced";

// httpd settings
var server;
const kDefaultServerPort = 9000;
const kBaseUrl = "http://localhost:" + kDefaultServerPort;
const kExamplePath = "/exampleTest/";
const kFailPath = "/fail/";
const kPingPath = "/ping/";
const kExampleURL = kBaseUrl + kExamplePath;
const kFailURL = kBaseUrl + kFailPath;
const kPingUrl = kBaseUrl + kPingPath;

// app/profile/firefox.js are not avaialble in xpcshell: hence, preset them
Services.prefs.setCharPref(kLocalePref, "en-US");
Services.prefs.setCharPref(kSourceUrlPref, kTestURL);
Services.prefs.setCharPref(kPingUrlPref, kPingUrl);
Services.prefs.setBoolPref(kNewtabEnhancedPref, true);

const kHttpHandlerData = {};
kHttpHandlerData[kExamplePath] = {"en-US": [{"url":"http://example.com","title":"RemoteSource"}]};

const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                              "nsIBinaryInputStream",
                              "setInputStream");

let gLastRequestPath;
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
    do_check_eq(actual, expected);
  }
  else if (typeof expected == "object") {
    // Make sure all the keys match up
    do_check_eq(Object.keys(actual).sort() + "", Object.keys(expected).sort());

    // Recursively check each value individually
    Object.keys(expected).forEach(key => {
      isIdentical(actual[key], expected[key]);
    });
  }
  else {
    do_check_eq(actual, expected);
  }
}

function fetchData() {
  let deferred = Promise.defer();

  DirectoryLinksProvider.getLinks(linkData => {
    deferred.resolve(linkData);
  });
  return deferred.promise;
}

function readJsonFile(jsonFile = DIRECTORY_LINKS_FILE) {
  let decoder = new TextDecoder();
  let directoryLinksFilePath = OS.Path.join(OS.Constants.Path.localProfileDir, jsonFile);
  return OS.File.read(directoryLinksFilePath).then(array => {
    let json = decoder.decode(array);
    return JSON.parse(json);
  }, () => { return "" });
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
  return Task.spawn(function() {
    let linksURL = options.linksURL || kTestURL;
    yield DirectoryLinksProvider.init();
    yield promiseDirectoryDownloadOnPrefChange(kLocalePref, options.locale || "en-US");
    yield promiseDirectoryDownloadOnPrefChange(kSourceUrlPref, linksURL);
    do_check_eq(DirectoryLinksProvider._linksURL, linksURL);
    DirectoryLinksProvider._lastDownloadMS = options.lastDownloadMS || 0;
  });
}

function promiseCleanDirectoryLinksProvider() {
  return Task.spawn(function() {
    yield promiseDirectoryDownloadOnPrefChange(kLocalePref, "en-US");
    yield promiseDirectoryDownloadOnPrefChange(kSourceUrlPref, kTestURL);
    DirectoryLinksProvider._lastDownloadMS  = 0;
    DirectoryLinksProvider.reset();
  });
}

function run_test() {
  // Set up a mock HTTP server to serve a directory page
  server = new HttpServer();
  server.registerPrefixHandler(kExamplePath, getHttpHandler(kExamplePath));
  server.registerPrefixHandler(kFailPath, getHttpHandler(kFailPath));
  server.start(kDefaultServerPort);

  run_next_test();

  // Teardown.
  do_register_cleanup(function() {
    server.stop(function() { });
    DirectoryLinksProvider.reset();
    Services.prefs.clearUserPref(kLocalePref);
    Services.prefs.clearUserPref(kSourceUrlPref);
    Services.prefs.clearUserPref(kPingUrlPref);
    Services.prefs.clearUserPref(kNewtabEnhancedPref);
  });
}

add_task(function test_reportSitesAction() {
  yield DirectoryLinksProvider.init();
  let deferred, expectedPath, expectedPost;
  let done = false;
  server.registerPrefixHandler(kPingPath, (aRequest, aResponse) => {
    if (done) {
      return;
    }

    do_check_eq(aRequest.path, expectedPath);

    let bodyStream = new BinaryInputStream(aRequest.bodyInputStream);
    let bodyObject = JSON.parse(NetUtil.readInputStreamToString(bodyStream, bodyStream.available()));
    isIdentical(bodyObject, expectedPost);

    deferred.resolve();
  });

  function sendPingAndTest(path, action, index) {
    deferred = Promise.defer();
    expectedPath = kPingPath + path;
    DirectoryLinksProvider.reportSitesAction(sites, action, index);
    return deferred.promise;
  }

  // Start with a single pinned link at position 3
  let sites = [,,{
    isPinned: _ => true,
    link: {
      directoryId: 1,
      frecency: 30000,
      url: "http://directory1/",
    },
  }];

  // Make sure we get the click ping for the directory link with fields we want
  // and unwanted fields removed by stringify/parse
  expectedPost = JSON.parse(JSON.stringify({
    click: 0,
    locale: "en-US",
    tiles: [{
      id: 1,
      pin: 1,
      pos: 2,
      score: 3,
      url: undefined,
    }],
  }));
  yield sendPingAndTest("click", "click", 2);

  // Try a pin click ping
  delete expectedPost.click;
  expectedPost.pin = 0;
  yield sendPingAndTest("click", "pin", 2);

  // Try a block click ping
  delete expectedPost.pin;
  expectedPost.block = 0;
  yield sendPingAndTest("click", "block", 2);

  // A view ping has no actions
  delete expectedPost.block;
  expectedPost.view = 0;
  yield sendPingAndTest("view", "view", 2);

  // Remove the identifier that makes it a directory link so just plain history
  delete sites[2].link.directoryId;
  delete expectedPost.tiles[0].id;
  yield sendPingAndTest("view", "view", 2);

  // Add directory link at position 0
  sites[0] = {
    isPinned: _ => false,
    link: {
      directoryId: 1234,
      frecency: 1000,
      url: "http://directory/",
    }
  };
  expectedPost.tiles.unshift(JSON.parse(JSON.stringify({
    id: 1234,
    pin: undefined,
    pos: undefined,
    score: undefined,
    url: undefined,
  })));
  expectedPost.view = 1;
  yield sendPingAndTest("view", "view", 2);

  // Make the history tile enhanced so it reports both id and url
  sites[2].enhancedId = "id from enhanced";
  expectedPost.tiles[1].id = "id from enhanced";
  expectedPost.tiles[1].url = "";
  yield sendPingAndTest("view", "view", 2);

  // Click the 0th site / 0th tile
  delete expectedPost.view;
  expectedPost.click = 0;
  yield sendPingAndTest("click", "click", 0);

  // Click the 2th site / 1th tile
  expectedPost.click = 1;
  yield sendPingAndTest("click", "click", 2);

  done = true;
});

add_task(function test_fetchAndCacheLinks_local() {
  yield DirectoryLinksProvider.init();
  yield cleanJsonFile();
  // Trigger cache of data or chrome uri files in profD
  yield DirectoryLinksProvider._fetchAndCacheLinks(kTestURL);
  let data = yield readJsonFile();
  isIdentical(data, kURLData);
});

add_task(function test_fetchAndCacheLinks_remote() {
  yield DirectoryLinksProvider.init();
  yield cleanJsonFile();
  // this must trigger directory links json download and save it to cache file
  yield promiseDirectoryDownloadOnPrefChange(kSourceUrlPref, kExampleURL + "%LOCALE%");
  do_check_eq(gLastRequestPath, kExamplePath + "en-US");
  let data = yield readJsonFile();
  isIdentical(data, kHttpHandlerData[kExamplePath]);
});

add_task(function test_fetchAndCacheLinks_malformedURI() {
  yield DirectoryLinksProvider.init();
  yield cleanJsonFile();
  let someJunk = "some junk";
  try {
    yield DirectoryLinksProvider._fetchAndCacheLinks(someJunk);
    do_throw("Malformed URIs should fail")
  } catch (e) {
    do_check_eq(e, "Error fetching " + someJunk)
  }

  // File should be empty.
  let data = yield readJsonFile();
  isIdentical(data, "");
});

add_task(function test_fetchAndCacheLinks_unknownHost() {
  yield DirectoryLinksProvider.init();
  yield cleanJsonFile();
  let nonExistentServer = "http://localhost:56789/";
  try {
    yield DirectoryLinksProvider._fetchAndCacheLinks(nonExistentServer);
    do_throw("BAD URIs should fail");
  } catch (e) {
    do_check_true(e.startsWith("Fetching " + nonExistentServer + " results in error code: "))
  }

  // File should be empty.
  let data = yield readJsonFile();
  isIdentical(data, "");
});

add_task(function test_fetchAndCacheLinks_non200Status() {
  yield DirectoryLinksProvider.init();
  yield cleanJsonFile();
  yield promiseDirectoryDownloadOnPrefChange(kSourceUrlPref, kFailURL);
  do_check_eq(gLastRequestPath, kFailPath);
  let data = yield readJsonFile();
  isIdentical(data, {});
});

// To test onManyLinksChanged observer, trigger a fetch
add_task(function test_DirectoryLinksProvider__linkObservers() {
  yield DirectoryLinksProvider.init();

  let testObserver = new LinksChangeObserver();
  DirectoryLinksProvider.addObserver(testObserver);
  do_check_eq(DirectoryLinksProvider._observers.size, 1);
  DirectoryLinksProvider._fetchAndCacheLinksIfNecessary(true);

  yield testObserver.deferred.promise;
  DirectoryLinksProvider._removeObservers();
  do_check_eq(DirectoryLinksProvider._observers.size, 0);

  yield promiseCleanDirectoryLinksProvider();
});

add_task(function test_linksURL_locale() {
  let data = {
    "en-US": [{url: "http://example.com", title: "US"}],
    "zh-CN": [
              {url: "http://example.net", title: "CN"},
              {url:"http://example.net/2", title: "CN2"}
    ],
  };
  let dataURI = 'data:application/json,' + JSON.stringify(data);

  yield promiseSetupDirectoryLinksProvider({linksURL: dataURI});

  let links;
  let expected_data;

  links = yield fetchData();
  do_check_eq(links.length, 1);
  expected_data = [{url: "http://example.com", title: "US", frecency: DIRECTORY_FRECENCY, lastVisitDate: 1}];
  isIdentical(links, expected_data);

  yield promiseDirectoryDownloadOnPrefChange("general.useragent.locale", "zh-CN");

  links = yield fetchData();
  do_check_eq(links.length, 2)
  expected_data = [
    {url: "http://example.net", title: "CN", frecency: DIRECTORY_FRECENCY, lastVisitDate: 2},
    {url: "http://example.net/2", title: "CN2", frecency: DIRECTORY_FRECENCY, lastVisitDate: 1}
  ];
  isIdentical(links, expected_data);

  yield promiseCleanDirectoryLinksProvider();
});

add_task(function test_DirectoryLinksProvider__prefObserver_url() {
  yield promiseSetupDirectoryLinksProvider({linksURL: kTestURL});

  let links = yield fetchData();
  do_check_eq(links.length, 1);
  let expectedData =  [{url: "http://example.com", title: "LocalSource", frecency: DIRECTORY_FRECENCY, lastVisitDate: 1}];
  isIdentical(links, expectedData);

  // tests these 2 things:
  // 1. _linksURL is properly set after the pref change
  // 2. invalid source url is correctly handled
  let exampleUrl = 'http://localhost:56789/bad';
  yield promiseDirectoryDownloadOnPrefChange(kSourceUrlPref, exampleUrl);
  do_check_eq(DirectoryLinksProvider._linksURL, exampleUrl);

  // since the download fail, the directory file must remain the same
  let newLinks = yield fetchData();
  isIdentical(newLinks, expectedData);

  // now remove the file, and re-download
  yield cleanJsonFile();
  yield promiseDirectoryDownloadOnPrefChange(kSourceUrlPref, exampleUrl + " ");
  // we now should see empty links
  newLinks = yield fetchData();
  isIdentical(newLinks, []);

  yield promiseCleanDirectoryLinksProvider();
});

add_task(function test_DirectoryLinksProvider_getLinks_noLocaleData() {
  yield promiseSetupDirectoryLinksProvider({locale: 'zh-CN'});
  let links = yield fetchData();
  do_check_eq(links.length, 0);
  yield promiseCleanDirectoryLinksProvider();
});

add_task(function test_DirectoryLinksProvider_getLinks_badData() {
  let data = {
    "en-US": {
      "en-US": [{url: "http://example.com", title: "US"}],
    },
  };
  let dataURI = 'data:application/json,' + JSON.stringify(data);
  yield promiseSetupDirectoryLinksProvider({linksURL: dataURI});

  // Make sure we get nothing for incorrectly formatted data
  let links = yield fetchData();
  do_check_eq(links.length, 0);
  yield promiseCleanDirectoryLinksProvider();
});

add_task(function test_DirectoryLinksProvider_needsDownload() {
  // test timestamping
  DirectoryLinksProvider._lastDownloadMS = 0;
  do_check_true(DirectoryLinksProvider._needsDownload);
  DirectoryLinksProvider._lastDownloadMS = Date.now();
  do_check_false(DirectoryLinksProvider._needsDownload);
  DirectoryLinksProvider._lastDownloadMS = Date.now() - (60*60*24 + 1)*1000;
  do_check_true(DirectoryLinksProvider._needsDownload);
  DirectoryLinksProvider._lastDownloadMS = 0;
});

add_task(function test_DirectoryLinksProvider_fetchAndCacheLinksIfNecessary() {
  yield DirectoryLinksProvider.init();
  yield cleanJsonFile();
  // explicitly change source url to cause the download during setup
  yield promiseSetupDirectoryLinksProvider({linksURL: kTestURL+" "});
  yield DirectoryLinksProvider._fetchAndCacheLinksIfNecessary();

  // inspect lastDownloadMS timestamp which should be 5 seconds less then now()
  let lastDownloadMS = DirectoryLinksProvider._lastDownloadMS;
  do_check_true((Date.now() - lastDownloadMS) < 5000);

  // we should have fetched a new file during setup
  let data = yield readJsonFile();
  isIdentical(data, kURLData);

  // attempt to download again - the timestamp should not change
  yield DirectoryLinksProvider._fetchAndCacheLinksIfNecessary();
  do_check_eq(DirectoryLinksProvider._lastDownloadMS, lastDownloadMS);

  // clean the file and force the download
  yield cleanJsonFile();
  yield DirectoryLinksProvider._fetchAndCacheLinksIfNecessary(true);
  data = yield readJsonFile();
  isIdentical(data, kURLData);

  // make sure that failed download does not corrupt the file, nor changes lastDownloadMS
  lastDownloadMS = DirectoryLinksProvider._lastDownloadMS;
  yield promiseDirectoryDownloadOnPrefChange(kSourceUrlPref, "http://");
  yield DirectoryLinksProvider._fetchAndCacheLinksIfNecessary(true);
  data = yield readJsonFile();
  isIdentical(data, kURLData);
  do_check_eq(DirectoryLinksProvider._lastDownloadMS, lastDownloadMS);

  // _fetchAndCacheLinksIfNecessary must return same promise if download is in progress
  let downloadPromise = DirectoryLinksProvider._fetchAndCacheLinksIfNecessary(true);
  let anotherPromise = DirectoryLinksProvider._fetchAndCacheLinksIfNecessary(true);
  do_check_true(downloadPromise === anotherPromise);
  yield downloadPromise;

  yield promiseCleanDirectoryLinksProvider();
});

add_task(function test_DirectoryLinksProvider_fetchDirectoryOnPrefChange() {
  yield DirectoryLinksProvider.init();

  let testObserver = new LinksChangeObserver();
  DirectoryLinksProvider.addObserver(testObserver);

  yield cleanJsonFile();
  // ensure that provider does not think it needs to download
  do_check_false(DirectoryLinksProvider._needsDownload);

  // change the source URL, which should force directory download
  yield promiseDirectoryDownloadOnPrefChange(kSourceUrlPref, kExampleURL);
  // then wait for testObserver to fire and test that json is downloaded
  yield testObserver.deferred.promise;
  do_check_eq(gLastRequestPath, kExamplePath);
  let data = yield readJsonFile();
  isIdentical(data, kHttpHandlerData[kExamplePath]);

  yield promiseCleanDirectoryLinksProvider();
});

add_task(function test_DirectoryLinksProvider_fetchDirectoryOnShow() {
  yield promiseSetupDirectoryLinksProvider();

  // set lastdownload to 0 to make DirectoryLinksProvider want to download
  DirectoryLinksProvider._lastDownloadMS = 0;
  do_check_true(DirectoryLinksProvider._needsDownload);

  // download should happen on view
  yield DirectoryLinksProvider.reportSitesAction([], "view");
  do_check_true(DirectoryLinksProvider._lastDownloadMS != 0);

  yield promiseCleanDirectoryLinksProvider();
});

add_task(function test_DirectoryLinksProvider_fetchDirectoryOnInit() {
  // ensure preferences are set to defaults
  yield promiseSetupDirectoryLinksProvider();
  // now clean to provider, so we can init it again
  yield promiseCleanDirectoryLinksProvider();

  yield cleanJsonFile();
  yield DirectoryLinksProvider.init();
  let data = yield readJsonFile();
  isIdentical(data, kURLData);

  yield promiseCleanDirectoryLinksProvider();
});

add_task(function test_DirectoryLinksProvider_getLinksFromCorruptedFile() {
  yield promiseSetupDirectoryLinksProvider();

  // write bogus json to a file and attempt to fetch from it
  let directoryLinksFilePath = OS.Path.join(OS.Constants.Path.profileDir, DIRECTORY_LINKS_FILE);
  yield OS.File.writeAtomic(directoryLinksFilePath, '{"en-US":');
  let data = yield fetchData();
  isIdentical(data, []);

  yield promiseCleanDirectoryLinksProvider();
});

add_task(function test_DirectoryLinksProvider_getEnhancedLink() {
  let data = {"en-US": [
    {url: "http://example.net", enhancedImageURI: "net1"},
    {url: "http://example.com", enhancedImageURI: "com1"},
    {url: "http://example.com", enhancedImageURI: "com2"},
  ]};
  let dataURI = 'data:application/json,' + JSON.stringify(data);
  yield promiseSetupDirectoryLinksProvider({linksURL: dataURI});

  let links = yield fetchData();
  do_check_eq(links.length, 3);

  function checkEnhanced(url, image) {
    let enhanced = DirectoryLinksProvider.getEnhancedLink({url: url});
    do_check_eq(enhanced && enhanced.enhancedImageURI, image);
  }

  // Get the expected image for the same site
  checkEnhanced("http://example.net/", "net1");
  checkEnhanced("http://example.net/path", "net1");
  checkEnhanced("https://www.example.net/", "net1");
  checkEnhanced("https://www3.example.net/", "net1");

  // Get the image of the last entry
  checkEnhanced("http://example.com", "com2");

  // Get the inline enhanced image
  let inline = DirectoryLinksProvider.getEnhancedLink({
    url: "http://example.com/echo",
    enhancedImageURI: "echo",
  });
  do_check_eq(inline.enhancedImageURI, "echo");
  do_check_eq(inline.url, "http://example.com/echo");

  // Undefined for not enhanced
  checkEnhanced("http://sub.example.net/", undefined);
  checkEnhanced("http://example.org", undefined);
  checkEnhanced("http://localhost", undefined);
  checkEnhanced("http://127.0.0.1", undefined);

  // Make sure old data is not cached
  data = {"en-US": [
    {url: "http://example.com", enhancedImageURI: "fresh"},
  ]};
  dataURI = 'data:application/json,' + JSON.stringify(data);
  yield promiseSetupDirectoryLinksProvider({linksURL: dataURI});
  links = yield fetchData();
  do_check_eq(links.length, 1);
  checkEnhanced("http://example.net", undefined);
  checkEnhanced("http://example.com", "fresh");
});

add_task(function test_DirectoryLinksProvider_setDefaultEnhanced() {
  function checkDefault(expected) {
    Services.prefs.clearUserPref(kNewtabEnhancedPref);
    do_check_eq(Services.prefs.getBoolPref(kNewtabEnhancedPref), expected);
  }

  // Use the default donottrack prefs (enabled = false, value = 1)
  Services.prefs.clearUserPref("privacy.donottrackheader.enabled");
  Services.prefs.clearUserPref("privacy.donottrackheader.value");
  checkDefault(true);

  // Turn on DNT - no track
  Services.prefs.setBoolPref("privacy.donottrackheader.enabled", true);
  checkDefault(false);

  // Set DNT - do track
  Services.prefs.setIntPref("privacy.donottrackheader.value", 0);
  checkDefault(true);

  // Turn off DNT header
  Services.prefs.clearUserPref("privacy.donottrackheader.enabled");
  checkDefault(true);

  // Clean up
  Services.prefs.clearUserPref("privacy.donottrackheader.value");
});
