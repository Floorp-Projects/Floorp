/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Http.jsm");
Cu.import("resource://testing-common/httpd.js");

XPCOMUtils.defineLazyModuleGetter(this, "MozLoopService",
                                  "resource:///modules/loop/MozLoopService.jsm");

const kMockWebSocketChannelName = "Mock WebSocket Channel";
const kWebSocketChannelContractID = "@mozilla.org/network/protocol;1?name=wss";

const kServerPushUrl = "http://localhost:3456";

// Fake loop server
var loopServer;

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

/**
 * Mock nsIWebSocketChannel for tests. This mocks the WebSocketChannel, and
 * enables us to check parameters and return messages similar to the push
 * server.
 */
let MockWebSocketChannel = function() {
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

  notify: function(version) {
    this.listener.onMessageAvailable(this.context,
      JSON.stringify({
        messageType: "notification", updates: [{
          channelID: "8b1081ce-9b35-42b5-b8f5-3ff8cb813a50",
          version: version
        }]
    }));
  },

  sendMsg: function(aMsg) {
    var message = JSON.parse(aMsg);

    switch(message.messageType) {
      case "hello":
        this.listener.onMessageAvailable(this.context,
          JSON.stringify({messageType: "hello"}));
        break;
      case "register":
        this.listener.onMessageAvailable(this.context,
          JSON.stringify({messageType: "register", pushEndpoint: "http://example.com/fake"}));
        break;
    }
  }
};

/**
 * The XPCOM factory for registering and creating the mock.
 */
let gMockWebSocketChannelFactory = {
  _registered: false,

  // We keep a list of instances so that we can access them outside of the service
  // that creates them.
  createdInstances: [],

  resetInstances: function() {
    this.createdInstances = [];
  },

  get CID() {
    let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
    return registrar.contractIDToCID(kWebSocketChannelContractID);
  },

  /**
   * Registers the MockWebSocketChannel, and stores the original.
   */
  register: function() {
    if (this._registered)
      return;

    let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

    this._origFactory = Components.manager
                                  .getClassObject(Cc[kWebSocketChannelContractID],
                                                  Ci.nsIFactory);

    registrar.unregisterFactory(Components.ID(this.CID),
                                this._origFactory);

    registrar.registerFactory(Components.ID(this.CID),
                              kMockWebSocketChannelName,
                              kWebSocketChannelContractID,
                              gMockWebSocketChannelFactory);

    this._registered = true;
  },

  /* Unregisters the MockWebSocketChannel, and re-registers the original
   * Prompt Service.
   */
  unregister: function() {
    if (!this._registered)
      return;

    let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

    registrar.unregisterFactory(Components.ID(this.CID),
                                gMockWebSocketChannelFactory);
    registrar.registerFactory(Components.ID(this.CID),
                              kMockWebSocketChannelName,
                              kWebSocketChannelContractID,
                              this._origFactory);

    delete this._origFactory;

    this._registered = false;
  },

  createInstance: function(outer, iid) {
    if (outer != null) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }

    let newChannel = new MockWebSocketChannel()

    this.createdInstances.push(newChannel);

    return newChannel;
  }
};
