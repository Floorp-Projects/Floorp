var Cu = Components.utils;
var Ci = Components.interfaces;
var Cc = Components.classes;

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var prefs = Cc["@mozilla.org/preferences-service;1"].
              getService(Ci.nsIPrefBranch);

// Since this test creates a TYPE_DOCUMENT channel via javascript, it will
// end up using the wrong LoadInfo constructor. Setting this pref will disable
// the ContentPolicyType assertion in the constructor.
prefs.setBoolPref("network.loadinfo.skip_type_assertion", true);

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

var httpserver =  null;
var channel = null;
var curTest = null;
var testpath = "/footpath";

var tests = [
  {
    description: "should not set request header for TYPE_OTHER",
    expectingHeader: false,
    contentType: Ci.nsIContentPolicy.TYPE_OTHER
  },
  {
    description: "should set request header for TYPE_DOCUMENT",
    expectingHeader: true,
    contentType: Ci.nsIContentPolicy.TYPE_DOCUMENT
  },
  {
    description: "should set request header for TYPE_SUBDOCUMENT",
    expectingHeader: true,
    contentType: Ci.nsIContentPolicy.TYPE_SUBDOCUMENT
  },
  {
    description: "should not set request header for TYPE_IMG",
    expectingHeader: false,
    contentType: Ci.nsIContentPolicy.TYPE_IMG
  },
];

function ChannelListener() {
}

ChannelListener.prototype = {
  onStartRequest: function(request, context) { },
  onDataAvailable: function(request, context, stream, offset, count) {
    do_throw("Should not get any data!");
  },
  onStopRequest: function(request, context, status) {
    var upgrade_insecure_header = false;
    try {
      if (request.getRequestHeader("Upgrade-Insecure-Requests")) {
        upgrade_insecure_header = true;
      }
    }
    catch (e) {
      // exception is thrown if header is not available on the request
    }
    // debug
    // dump("executing test: " + curTest.description);
    Assert.equal(upgrade_insecure_header, curTest.expectingHeader)
    run_next_test();
  },
};

function setupChannel(aContentType) {
  var chan = NetUtil.newChannel({
    uri: URL + testpath,
    loadUsingSystemPrincipal: true,
    contentPolicyType: aContentType
  });
  chan.QueryInterface(Ci.nsIHttpChannel);
  chan.requestMethod = "GET";
  return chan;
}

function serverHandler(metadata, response) {
  // no need to perform anything here
}

function run_next_test() {
  curTest = tests.shift();
  if (!curTest) {
    httpserver.stop(do_test_finished);
    return;
  }
  channel = setupChannel(curTest.contentType);
  channel.asyncOpen2(new ChannelListener());
}

function run_test() {
  // set up the test environment
  httpserver = new HttpServer();
  httpserver.registerPathHandler(testpath, serverHandler);
  httpserver.start(-1);

  run_next_test();
  do_test_pending();
}
