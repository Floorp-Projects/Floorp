/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Http.jsm");
Cu.import("resource://testing-common/httpd.js");
Cu.import("resource:///modules/loop/MozLoopService.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
const { MozLoopServiceInternal } = Cu.import("resource:///modules/loop/MozLoopService.jsm", {});

XPCOMUtils.defineLazyModuleGetter(this, "MozLoopPushHandler",
                                  "resource:///modules/loop/MozLoopPushHandler.jsm");

const kMockWebSocketChannelName = "Mock WebSocket Channel";
const kWebSocketChannelContractID = "@mozilla.org/network/protocol;1?name=wss";

const kServerPushUrl = "http://localhost:3456";
const kEndPointUrl = "http://example.com/fake";
const kUAID = "f47ac11b-58ca-4372-9567-0e02b2c3d479";

// Fake loop server
var loopServer;

// Ensure loop is always enabled for tests
Services.prefs.setBoolPref("loop.enabled", true);
Services.prefs.setBoolPref("loop.throttled", false);

function setupFakeLoopServer() {
  loopServer = new HttpServer();
  loopServer.start(-1);

  Services.prefs.setCharPref("services.push.serverURL", kServerPushUrl);

  Services.prefs.setCharPref("loop.server",
    "http://localhost:" + loopServer.identity.primaryPort);

  do_register_cleanup(function() {
    loopServer.stop(function() {});
  });
}

function waitForCondition(aConditionFn, aMaxTries=50, aCheckInterval=100) {
  function tryAgain() {
    function tryNow() {
      tries++;
      if (aConditionFn()) {
        deferred.resolve();
      } else if (tries < aMaxTries) {
        tryAgain();
      } else {
        deferred.reject("Condition timed out: " + aConditionFn.toSource());
      }
    }
    do_timeout(aCheckInterval, tryNow);
  }
  let deferred = Promise.defer();
  let tries = 0;
  tryAgain();
  return deferred.promise;
}

function getLoopString(stringID) {
  return MozLoopServiceInternal.localizedStrings[stringID].textContent;
}

/**
 * This is used to fake push registration and notifications for
 * MozLoopService tests. There is only one object created per test instance, as
 * once registration has taken place, the object cannot currently be changed.
 */
let mockPushHandler = {
  // This sets the registration result to be returned when initialize
  // is called. By default, it is equivalent to success.
  registrationResult: null,
  registrationPushURL: undefined,

  /**
   * MozLoopPushHandler API
   */
  initialize: function(registerCallback, notificationCallback) {
    registerCallback(this.registrationResult, this.registrationPushURL);
    this._notificationCallback = notificationCallback;
  },

  /**
   * Test-only API to simplify notifying a push notification result.
   */
  notify: function(version) {
    this._notificationCallback(version);
  }
};

/**
 * Mock nsIWebSocketChannel for tests. This mocks the WebSocketChannel, and
 * enables us to check parameters and return messages similar to the push
 * server.
 */
let MockWebSocketChannel = function(options) {
  let _options = options || {};
  this.defaultMsgHandler = _options.defaultMsgHandler;
};

MockWebSocketChannel.prototype = {
  QueryInterface: XPCOMUtils.generateQI(Ci.nsIWebSocketChannel),

  /**
   * nsIWebSocketChannel implementations.
   * See nsIWebSocketChannel.idl for API details.
   */
  asyncOpen: function(aURI, aOrigin, aListener, aContext) {
    this.uri = aURI;
    this.origin = aOrigin;
    this.listener = aListener;
    this.context = aContext;

    this.listener.onStart(this.context);
  },

  sendMsg: function(aMsg) {
    var message = JSON.parse(aMsg);

    switch(message.messageType) {
      case "hello":
        this.listener.onMessageAvailable(this.context,
          JSON.stringify({messageType: "hello",
                          uaid: kUAID}));
        break;
      case "register":
        this.channelID = message.channelID;
        let statusCode = 200;
        if (this.initRegStatus) {
          statusCode = this.initRegStatus;
          this.initRegStatus = 0;
        }
        this.listener.onMessageAvailable(this.context,
          JSON.stringify({messageType: "register",
                          status: statusCode,
                          channelID: this.channelID,
                          pushEndpoint: kEndPointUrl}));
        break;
      default:
        this.defaultMsgHandler && this.defaultMsgHandler(message);
    }
  },

  notify: function(version) {
    this.listener.onMessageAvailable(this.context,
      JSON.stringify({
        messageType: "notification", updates: [{
          channelID: this.channelID,
          version: version
        }]
    }));
  },

  stop: function (err) {
    this.listener.onStop(this.context, err || -1);
  },

  serverClose: function (err) {
    this.listener.onServerClose(this.context, err || -1);
  },
};
