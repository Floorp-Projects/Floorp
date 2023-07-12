/*
 * Test to ensure that image loading/decoding notifications are always
 * delivered async, and in the order we expect.
 *
 * Must be included from a file that has a uri of the image to test defined in
 * var uri.
 */
/* import-globals-from image_load_helpers.js */

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);
const { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);
const ReferrerInfo = Components.Constructor(
  "@mozilla.org/referrer-info;1",
  "nsIReferrerInfo",
  "init"
);

var server = new HttpServer();
server.registerDirectory("/", do_get_file(""));
server.registerContentType("sjs", "sjs");
server.start(-1);

load("image_load_helpers.js");

var requests = [];
/* global uri */

// Return a closure that holds on to the listener from the original
// imgIRequest, and compares its results to the cloned one.
function getCloneStopCallback(original_listener) {
  return function cloneStop(listener) {
    Assert.equal(original_listener.state, listener.state);

    // Sanity check to make sure we didn't accidentally use the same listener
    // twice.
    Assert.notEqual(original_listener, listener);
    do_test_finished();
  };
}

// Make sure that cloned requests get all the same callbacks as the original,
// but they aren't synchronous right now.
function checkClone(other_listener, aRequest) {
  do_test_pending();

  // For as long as clone notification is synchronous, we can't test the clone state reliably.
  var listener = new ImageListener(
    null,
    function (foo, bar) {
      do_test_finished();
    } /* getCloneStopCallback(other_listener)*/
  );
  listener.synchronous = false;
  var outer = Cc["@mozilla.org/image/tools;1"]
    .getService(Ci.imgITools)
    .createScriptedObserver(listener);
  var clone = aRequest.clone(outer);
  requests.push({ request: clone, locked: false });
}

// Ensure that all the callbacks were called on aRequest.
function checkSizeAndLoad(listener, aRequest) {
  Assert.notEqual(listener.state & SIZE_AVAILABLE, 0);
  Assert.notEqual(listener.state & LOAD_COMPLETE, 0);

  do_test_finished();
}

function secondLoadDone(oldlistener, aRequest) {
  do_test_pending();

  try {
    var staticrequest = aRequest.getStaticRequest();

    // For as long as clone notification is synchronous, we can't test the
    // clone state reliably.
    var listener = new ImageListener(null, checkSizeAndLoad);
    listener.synchronous = false;
    var outer = Cc["@mozilla.org/image/tools;1"]
      .getService(Ci.imgITools)
      .createScriptedObserver(listener);
    var staticrequestclone = staticrequest.clone(outer);
    requests.push({ request: staticrequestclone, locked: false });
  } catch (e) {
    // We can't create a static request. Most likely the request we started
    // with didn't load successfully.
    do_test_finished();
  }

  run_loadImageWithChannel_tests();

  do_test_finished();
}

// Load the request a second time. This should come from the image cache, and
// therefore would be at most risk of being served synchronously.
function checkSecondLoad() {
  do_test_pending();

  var listener = new ImageListener(checkClone, secondLoadDone);
  var outer = Cc["@mozilla.org/image/tools;1"]
    .getService(Ci.imgITools)
    .createScriptedObserver(listener);
  var referrerInfo = new ReferrerInfo(
    Ci.nsIReferrerInfo.NO_REFERRER_WHEN_DOWNGRADE,
    true,
    null
  );
  requests.push({
    request: gCurrentLoader.loadImageXPCOM(
      uri,
      null,
      referrerInfo,
      null,
      null,
      outer,
      null,
      0,
      null
    ),
    locked: false,
  });
  listener.synchronous = false;
}

function firstLoadDone(oldlistener, aRequest) {
  checkSecondLoad(uri);

  do_test_finished();
}

// Return a closure that allows us to check the stream listener's status when the
// image finishes loading.
function getChannelLoadImageStopCallback(streamlistener, next) {
  return function channelLoadStop(imglistener, aRequest) {
    next();

    do_test_finished();
  };
}

// Load the request a second time. This should come from the image cache, and
// therefore would be at most risk of being served synchronously.
function checkSecondChannelLoad() {
  do_test_pending();
  var channel = NetUtil.newChannel({ uri, loadUsingSystemPrincipal: true });
  var channellistener = new ChannelListener();
  channel.asyncOpen(channellistener);

  var listener = new ImageListener(
    null,
    getChannelLoadImageStopCallback(channellistener, all_done_callback)
  );
  var outer = Cc["@mozilla.org/image/tools;1"]
    .getService(Ci.imgITools)
    .createScriptedObserver(listener);
  var outlistener = {};
  requests.push({
    request: gCurrentLoader.loadImageWithChannelXPCOM(
      channel,
      outer,
      null,
      outlistener
    ),
    locked: false,
  });
  channellistener.outputListener = outlistener.value;

  listener.synchronous = false;
}

function run_loadImageWithChannel_tests() {
  // To ensure we're testing what we expect to, create a new loader and cache.
  gCurrentLoader = Cc["@mozilla.org/image/loader;1"].createInstance(
    Ci.imgILoader
  );

  do_test_pending();
  var channel = NetUtil.newChannel({ uri, loadUsingSystemPrincipal: true });
  var channellistener = new ChannelListener();
  channel.asyncOpen(channellistener);

  var listener = new ImageListener(
    null,
    getChannelLoadImageStopCallback(channellistener, checkSecondChannelLoad)
  );
  var outer = Cc["@mozilla.org/image/tools;1"]
    .getService(Ci.imgITools)
    .createScriptedObserver(listener);
  var outlistener = {};
  requests.push({
    request: gCurrentLoader.loadImageWithChannelXPCOM(
      channel,
      outer,
      null,
      outlistener
    ),
    locked: false,
  });
  channellistener.outputListener = outlistener.value;

  listener.synchronous = false;
}

function all_done_callback() {
  server.stop(function () {
    do_test_finished();
  });
}

function startImageCallback(otherCb) {
  return function (listener, request) {
    // Make sure we can load the same image immediately out of the cache.
    do_test_pending();
    var listener2 = new ImageListener(null, function (foo, bar) {
      do_test_finished();
    });
    var outer = Cc["@mozilla.org/image/tools;1"]
      .getService(Ci.imgITools)
      .createScriptedObserver(listener2);
    var referrerInfo = new ReferrerInfo(
      Ci.nsIReferrerInfo.NO_REFERRER_WHEN_DOWNGRADE,
      true,
      null
    );
    requests.push({
      request: gCurrentLoader.loadImageXPCOM(
        uri,
        null,
        referrerInfo,
        null,
        null,
        outer,
        null,
        0,
        null
      ),
      locked: false,
    });
    listener2.synchronous = false;

    // Now that we've started another load, chain to the callback.
    otherCb(listener, request);
  };
}

var gCurrentLoader;

function cleanup() {
  for (let { request, locked } of requests) {
    if (locked) {
      try {
        request.unlockImage();
      } catch (e) {}
    }
    request.cancelAndForgetObserver(0);
  }
}

function run_test() {
  registerCleanupFunction(cleanup);

  gCurrentLoader = Cc["@mozilla.org/image/loader;1"].createInstance(
    Ci.imgILoader
  );

  do_test_pending();
  var listener = new ImageListener(
    startImageCallback(checkClone),
    firstLoadDone
  );
  var outer = Cc["@mozilla.org/image/tools;1"]
    .getService(Ci.imgITools)
    .createScriptedObserver(listener);
  var referrerInfo = new ReferrerInfo(
    Ci.nsIReferrerInfo.NO_REFERRER_WHEN_DOWNGRADE,
    true,
    null
  );
  var req = gCurrentLoader.loadImageXPCOM(
    uri,
    null,
    referrerInfo,
    null,
    null,
    outer,
    null,
    0,
    null
  );

  // Ensure that we don't cause any mayhem when we lock an image.
  req.lockImage();

  requests.push({ request: req, locked: true });

  listener.synchronous = false;
}
