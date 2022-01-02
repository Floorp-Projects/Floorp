// Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
var prefetch = Cc["@mozilla.org/prefetch-service;1"].getService(
  Ci.nsIPrefetchService
);

var ReferrerInfo = Components.Constructor(
  "@mozilla.org/referrer-info;1",
  "nsIReferrerInfo",
  "init"
);

var ios = Services.io;
var prefs = Services.prefs;

var parser = new DOMParser();

var doc;

var docbody =
  '<html xmlns="http://www.w3.org/1999/xhtml"><head></head><body>' +
  '<link id="node1"/><link id="node2"/>' +
  "</body></html>";

var node1;
var node2;

function run_test() {
  prefs.setBoolPref("network.prefetch-next", true);

  doc = parser.parseFromString(docbody, "text/html");

  node1 = doc.getElementById("node1");
  node2 = doc.getElementById("node2");

  run_next_test();
}

add_test(function test_cancel1() {
  var uri = ios.newURI("http://localhost/1");
  var referrerInfo = new ReferrerInfo(Ci.nsIReferrerInfo.EMPTY, true, uri);

  prefetch.prefetchURI(uri, referrerInfo, node1, true);

  Assert.ok(prefetch.hasMoreElements(), "There is a request in the queue");

  // Trying to prefetch again the same uri with the same node will fail.
  var didFail = 0;

  try {
    prefetch.prefetchURI(uri, referrerInfo, node1, true);
  } catch (e) {
    didFail = 1;
  }

  Assert.ok(
    didFail == 1,
    "Prefetching the same request with the same node fails."
  );

  Assert.ok(prefetch.hasMoreElements(), "There is still request in the queue");

  prefetch.cancelPrefetchPreloadURI(uri, node1);

  Assert.ok(!prefetch.hasMoreElements(), "There is no request in the queue");
  run_next_test();
});

add_test(function test_cancel2() {
  // Prefetch a uri with 2 different nodes. There should be 2 request
  // in the queue and canceling one will not cancel the other.

  var uri = ios.newURI("http://localhost/1");
  var referrerInfo = new ReferrerInfo(Ci.nsIReferrerInfo.EMPTY, true, uri);

  prefetch.prefetchURI(uri, referrerInfo, node1, true);
  prefetch.prefetchURI(uri, referrerInfo, node2, true);

  Assert.ok(prefetch.hasMoreElements(), "There are requests in the queue");

  prefetch.cancelPrefetchPreloadURI(uri, node1);

  Assert.ok(
    prefetch.hasMoreElements(),
    "There is still one more request in the queue"
  );

  prefetch.cancelPrefetchPreloadURI(uri, node2);

  Assert.ok(!prefetch.hasMoreElements(), "There is no request in the queue");
  run_next_test();
});

add_test(function test_cancel3() {
  // Request a prefetch of a uri. Trying to cancel a prefetch for the same uri
  // with a different node will fail.
  var uri = ios.newURI("http://localhost/1");
  var referrerInfo = new ReferrerInfo(Ci.nsIReferrerInfo.EMPTY, true, uri);

  prefetch.prefetchURI(uri, referrerInfo, node1, true);

  Assert.ok(prefetch.hasMoreElements(), "There is a request in the queue");

  var didFail = 0;

  try {
    prefetch.cancelPrefetchPreloadURI(uri, node2, true);
  } catch (e) {
    didFail = 1;
  }
  Assert.ok(didFail == 1, "Canceling the request failed");

  Assert.ok(
    prefetch.hasMoreElements(),
    "There is still a request in the queue"
  );

  prefetch.cancelPrefetchPreloadURI(uri, node1);
  Assert.ok(!prefetch.hasMoreElements(), "There is no request in the queue");
  run_next_test();
});

add_test(function test_cancel4() {
  // Request a prefetch of a uri. Trying to cancel a prefetch for a different uri
  // with the same node will fail.
  var uri1 = ios.newURI("http://localhost/1");
  var uri2 = ios.newURI("http://localhost/2");
  var referrerInfo = new ReferrerInfo(Ci.nsIReferrerInfo.EMPTY, true, uri1);

  prefetch.prefetchURI(uri1, referrerInfo, node1, true);

  Assert.ok(prefetch.hasMoreElements(), "There is a request in the queue");

  var didFail = 0;

  try {
    prefetch.cancelPrefetchPreloadURI(uri2, node1);
  } catch (e) {
    didFail = 1;
  }
  Assert.ok(didFail == 1, "Canceling the request failed");

  Assert.ok(
    prefetch.hasMoreElements(),
    "There is still a request in the queue"
  );

  prefetch.cancelPrefetchPreloadURI(uri1, node1);
  Assert.ok(!prefetch.hasMoreElements(), "There is no request in the queue");
  run_next_test();
});
