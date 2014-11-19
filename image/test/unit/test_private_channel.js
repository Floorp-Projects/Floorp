const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://testing-common/httpd.js");

var server = new HttpServer();
server.registerPathHandler('/image.png', imageHandler);
server.start(-1);

load('image_load_helpers.js');

var gHits = 0;

var gIoService = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
var gPublicLoader = Cc["@mozilla.org/image/loader;1"].createInstance(Ci.imgILoader);
var gPrivateLoader = Cc["@mozilla.org/image/loader;1"].createInstance(Ci.imgILoader);
gPrivateLoader.QueryInterface(Ci.imgICache).respectPrivacyNotifications();

function imageHandler(metadata, response) {
  gHits++;
  response.setHeader("Cache-Control", "max-age=10000", false);
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "image/png", false);
  var body = "iVBORw0KGgoAAAANSUhEUgAAAAMAAAADCAIAAADZSiLoAAAAEUlEQVQImWP4z8AAQTAamQkAhpcI+DeMzFcAAAAASUVORK5CYII=";
  response.bodyOutputStream.write(body, body.length);
}

var requests = [];
var listeners = [];

function NotificationCallbacks(isPrivate) {
  this.usePrivateBrowsing = isPrivate;
}

NotificationCallbacks.prototype = {
  QueryInterface: function (iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsILoadContext))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
  getInterface: function(iid) {
    if (iid.equals(Ci.nsILoadContext))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

var gImgPath = 'http://localhost:' + server.identity.primaryPort + '/image.png';

function setup_chan(path, isPrivate, callback) {
  var uri = gIoService.newURI(gImgPath, null, null);
  var chan = gIoService.newChannelFromURI(uri);
  chan.notificationCallbacks = new NotificationCallbacks(isPrivate);
  var channelListener = new ChannelListener();
  chan.asyncOpen(channelListener, null);

  var listener = new ImageListener(null, callback);
  var outlistener = {};
  var loader = isPrivate ? gPrivateLoader : gPublicLoader;
  var outer = Cc["@mozilla.org/image/tools;1"].getService(Ci.imgITools)
                .createScriptedObserver(listener);
  listeners.push(outer);
  requests.push(loader.loadImageWithChannelXPCOM(chan, outer, null, outlistener));
  channelListener.outputListener = outlistener.value;
  listener.synchronous = false;
}

function loadImage(isPrivate, callback) {
  var listener = new ImageListener(null, callback);
  var outer = Cc["@mozilla.org/image/tools;1"].getService(Ci.imgITools)
                .createScriptedObserver(listener);
  var uri = gIoService.newURI(gImgPath, null, null);
  var loadGroup = Cc["@mozilla.org/network/load-group;1"].createInstance(Ci.nsILoadGroup);
  loadGroup.notificationCallbacks = new NotificationCallbacks(isPrivate);
  var loader = isPrivate ? gPrivateLoader : gPublicLoader;
  requests.push(loader.loadImageXPCOM(uri, null, null, "default", null, loadGroup, outer, null, 0, null));
  listener.synchronous = false;
}

function run_loadImage_tests() {
  function observer() {
    Services.obs.removeObserver(observer, "cacheservice:empty-cache");
    gHits = 0;
    loadImage(false, function() {
      loadImage(false, function() {
        loadImage(true, function() {
          loadImage(true, function() {
            do_check_eq(gHits, 2);
            server.stop(do_test_finished);
          });
        });
      });
    });
  }

  Services.obs.addObserver(observer, "cacheservice:empty-cache", false);
  let cs = Cc["@mozilla.org/netwerk/cache-storage-service;1"]
             .getService(Ci.nsICacheStorageService);
  cs.clear();
}

function cleanup()
{
  for (var i = 0; i < requests.length; ++i) {
    requests[i].cancelAndForgetObserver(0);
  }
}

function run_test() {
  do_register_cleanup(cleanup);

  do_test_pending();

  // We create a public channel that loads an image, then an identical
  // one that should cause a cache read. We then create a private channel
  // and load the same image, and do that a second time to ensure a cache
  // read. In total, we should cause two separate http responses to occur,
  // since the private channels shouldn't be able to use the public cache.
  setup_chan('/image.png', false, function() {
    setup_chan('/image.png', false, function() {
      setup_chan('/image.png', true, function() {
        setup_chan('/image.png', true, function() {
          do_check_eq(gHits, 2);
          run_loadImage_tests();
        });
      });
    });
  });
}
