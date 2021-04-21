const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyGetter(this, "HTTP_TEST_URL", function() {
  return "http://test1.example.com";
});

const TEST_PATH = "/https_only_https_first_path";
var httpserver = null;
var channel = null;
var curTest = null;

const TESTS = [
  {
    // Test 1: all prefs to false
    description: "Test 1 - top-level",
    contentType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
    https_only: false,
    https_only_pbm: false,
    https_first: false,
    https_first_pbm: false,
    pbm: false,
    expectedScheme: "http",
  },
  {
    description: "Test 1 - top-level - pbm",
    contentType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
    https_only: false,
    https_only_pbm: false,
    https_first: false,
    https_first_pbm: false,
    pbm: true,
    expectedScheme: "http",
  },
  {
    description: "Test 1 - sub-resource",
    contentType: Ci.nsIContentPolicy.TYPE_IMAGE,
    https_only: false,
    https_only_pbm: false,
    https_first: false,
    https_first_pbm: false,
    pbm: false,
    expectedScheme: "http",
  },
  {
    description: "Test 1 - sub-resource - pbm",
    contentType: Ci.nsIContentPolicy.TYPE_IMAGE,
    https_only: false,
    https_only_pbm: false,
    https_first: false,
    https_first_pbm: false,
    pbm: true,
    expectedScheme: "http",
  },
  // Test 2: https_only true
  {
    description: "Test 2 - top-level",
    contentType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
    https_only: true,
    https_only_pbm: false,
    https_first: false,
    https_first_pbm: false,
    pbm: false,
    expectedScheme: "https",
  },
  {
    description: "Test 2 - top-level - pbm",
    contentType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
    https_only: true,
    https_only_pbm: false,
    https_first: false,
    https_first_pbm: false,
    pbm: true,
    expectedScheme: "https",
  },
  {
    description: "Test 2 - sub-resource",
    contentType: Ci.nsIContentPolicy.TYPE_IMAGE,
    https_only: true,
    https_only_pbm: false,
    https_first: false,
    https_first_pbm: false,
    pbm: false,
    expectedScheme: "https",
  },
  {
    description: "Test 2 - sub-resource - pbm",
    contentType: Ci.nsIContentPolicy.TYPE_IMAGE,
    https_only: true,
    https_only_pbm: false,
    https_first: false,
    https_first_pbm: false,
    pbm: true,
    expectedScheme: "https",
  },
  // Test 3: https_only_pbm true
  {
    description: "Test 3 - top-level",
    contentType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
    https_only: false,
    https_only_pbm: true,
    https_first: false,
    https_first_pbm: false,
    pbm: false,
    expectedScheme: "http",
  },
  {
    description: "Test 3 - top-level - pbm",
    contentType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
    https_only: false,
    https_only_pbm: true,
    https_first: false,
    https_first_pbm: false,
    pbm: true,
    expectedScheme: "https",
  },
  {
    description: "Test 3 - sub-resource",
    contentType: Ci.nsIContentPolicy.TYPE_IMAGE,
    https_only: false,
    https_only_pbm: true,
    https_first: false,
    https_first_pbm: false,
    pbm: false,
    expectedScheme: "http",
  },
  {
    description: "Test 3 - sub-resource - pbm",
    contentType: Ci.nsIContentPolicy.TYPE_IMAGE,
    https_only: false,
    https_only_pbm: true,
    https_first: false,
    https_first_pbm: false,
    pbm: true,
    expectedScheme: "https",
  },
  // Test 4: https_first true
  {
    description: "Test 4 - top-level",
    contentType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
    https_only: false,
    https_only_pbm: false,
    https_first: true,
    https_first_pbm: false,
    pbm: false,
    expectedScheme: "https",
  },
  {
    description: "Test 4 - top-level - pbm",
    contentType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
    https_only: false,
    https_only_pbm: false,
    https_first: true,
    https_first_pbm: false,
    pbm: true,
    expectedScheme: "https",
  },
  {
    description: "Test 4 - sub-resource",
    contentType: Ci.nsIContentPolicy.TYPE_IMAGE,
    https_only: false,
    https_only_pbm: false,
    https_first: true,
    https_first_pbm: false,
    pbm: false,
    expectedScheme: "http",
  },
  {
    description: "Test 4 - sub-resource - pbm",
    contentType: Ci.nsIContentPolicy.TYPE_IMAGE,
    https_only: false,
    https_only_pbm: false,
    https_first: true,
    https_first_pbm: false,
    pbm: true,
    expectedScheme: "http",
  },
  // Test 5: https_first_pbm true
  {
    description: "Test 5 - top-level",
    contentType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
    https_only: false,
    https_only_pbm: false,
    https_first: false,
    https_first_pbm: true,
    pbm: false,
    expectedScheme: "http",
  },
  {
    description: "Test 5 - top-level - pbm",
    contentType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
    https_only: false,
    https_only_pbm: false,
    https_first: false,
    https_first_pbm: true,
    pbm: true,
    expectedScheme: "https",
  },
  {
    description: "Test 5 - sub-resource",
    contentType: Ci.nsIContentPolicy.TYPE_IMAGE,
    https_only: false,
    https_only_pbm: false,
    https_first: false,
    https_first_pbm: true,
    pbm: false,
    expectedScheme: "http",
  },
  {
    description: "Test 5 - sub-resource - pbm",
    contentType: Ci.nsIContentPolicy.TYPE_IMAGE,
    https_only: false,
    https_only_pbm: false,
    https_first: false,
    https_first_pbm: true,
    pbm: true,
    expectedScheme: "http",
  },
  // Test 6: https_only overrules https_first
  {
    description: "Test 6 - top-level",
    contentType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
    https_only: true,
    https_only_pbm: false,
    https_first: true,
    https_first_pbm: false,
    pbm: false,
    expectedScheme: "https",
  },
  {
    description: "Test 6 - top-level - pbm",
    contentType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
    https_only: true,
    https_only_pbm: false,
    https_first: true,
    https_first_pbm: false,
    pbm: true,
    expectedScheme: "https",
  },
  {
    description: "Test 6 - sub-resource",
    contentType: Ci.nsIContentPolicy.TYPE_IMAGE,
    https_only: true,
    https_only_pbm: false,
    https_first: true,
    https_first_pbm: false,
    pbm: false,
    expectedScheme: "https",
  },
  {
    description: "Test 6 - sub-resource - pbm",
    contentType: Ci.nsIContentPolicy.TYPE_IMAGE,
    https_only: true,
    https_only_pbm: false,
    https_first: true,
    https_first_pbm: false,
    pbm: true,
    expectedScheme: "https",
  },
];

function ChannelListener() {}

ChannelListener.prototype = {
  onStartRequest(request) {
    // dummy implementation
  },
  onDataAvailable(request, stream, offset, count) {
    do_throw("Should not get any data!");
  },
  onStopRequest(request, status) {
    var chan = request.QueryInterface(Ci.nsIChannel);
    let requestURL = chan.URI;
    Assert.equal(
      requestURL.scheme,
      curTest.expectedScheme,
      curTest.description
    );
    Assert.equal(requestURL.host, "test1.example.com", curTest.description);
    Assert.equal(requestURL.filePath, TEST_PATH, curTest.description);
    run_next_test();
  },
};

function setUpPrefs() {
  // set up the required prefs
  Services.prefs.setBoolPref(
    "dom.security.https_only_mode",
    curTest.https_only
  );
  Services.prefs.setBoolPref(
    "dom.security.https_only_mode_pbm",
    curTest.https_only_pbm
  );
  Services.prefs.setBoolPref("dom.security.https_first", curTest.https_first);
  Services.prefs.setBoolPref(
    "dom.security.https_first_pbm",
    curTest.https_first_pbm
  );
}

function setUpChannel() {
  // 1) Set up Principal using OA in case of Private Browsing
  let attr = {};
  if (curTest.pbm) {
    attr.privateBrowsingId = 1;
  }
  let uri = Services.io.newURI("http://test1.example.com");
  let principal = Services.scriptSecurityManager.createContentPrincipal(
    uri,
    attr
  );

  // 2) Set up Channel
  var chan = NetUtil.newChannel({
    uri: HTTP_TEST_URL + TEST_PATH,
    loadingPrincipal: principal,
    contentPolicyType: curTest.contentType,
    securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
  });
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.requestMethod = "GET";
  return chan;
}

function serverHandler(metadata, response) {
  // dummy implementation
}

function run_next_test() {
  curTest = TESTS.shift();
  if (!curTest) {
    httpserver.stop(do_test_finished);
    return;
  }

  setUpPrefs();

  channel = setUpChannel();
  channel.asyncOpen(new ChannelListener());
}

function run_test() {
  do_get_profile();

  // set up the test environment
  httpserver = new HttpServer();
  httpserver.registerPathHandler(TEST_PATH, serverHandler);
  httpserver.start(-1);

  run_next_test();
  do_test_pending();
}
