"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

/**
 * Defers one or more callbacks until the next turn of the event loop. Multiple
 * callbacks are executed in order.
 *
 * @param {Function[]} callbacks The callbacks to execute. One callback will be
 *  executed per tick.
 */
function waterfall(...callbacks) {
  callbacks.reduce((promise, callback) => promise.then(() => {
    callback();
  }), Promise.resolve()).catch(Cu.reportError);
}

/**
 * Minimal implementation of a mock WebSocket connect to be used with
 * PushService. Forwards and receive messages from the implementation
 * that lives in the content process.
 */
function MockWebSocketParent(originalURI) {
  this._originalURI = originalURI;
}

MockWebSocketParent.prototype = {
  _originalURI: null,

  _listener: null,
  _context: null,

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsISupports,
    Ci.nsIWebSocketChannel
  ]),

  get originalURI() {
    return this._originalURI;
  },

  asyncOpen(uri, origin, windowId, listener, context) {
    this._listener = listener;
    this._context = context;
    waterfall(() => this._listener.onStart(this._context));
  },

  sendMsg(msg) {
    sendAsyncMessage("socket-client-msg", msg);
  },

  close() {
    waterfall(() => this._listener.onStop(this._context, Cr.NS_OK));
  },

  serverSendMsg(msg) {
    waterfall(() => this._listener.onMessageAvailable(this._context, msg),
              () => this._listener.onAcknowledge(this._context, 0));
  },
};

function MockNetworkInfo() {}

MockNetworkInfo.prototype = {
  getNetworkInformation() {
    return {mcc: '', mnc: '', ip: ''};
  },

  getNetworkState(callback) {
    callback({mcc: '', mnc: '', ip: '', netid: ''});
  },

  getNetworkStateChangeEventName() {
    return 'network:offline-status-changed';
  }
};

var pushService = Cc["@mozilla.org/push/Service;1"].
                  getService(Ci.nsIPushService).
                  wrappedJSObject;

var mockWebSocket;

addMessageListener("socket-setup", function () {
  mockWebSocket = new Promise((resolve, reject) => {
    var mockSocket = null;
    pushService.replaceServiceBackend({
      serverURI: "wss://push.example.org/",
      networkInfo: new MockNetworkInfo(),
      makeWebSocket(uri) {
        if (!mockSocket) {
          mockSocket = new MockWebSocketParent(uri);
          resolve(mockSocket);
        }

        return mockSocket;
      }
    });
  });
});

addMessageListener("socket-teardown", function () {
  mockWebSocket.then(socket => {
    socket.close();
    pushService.restoreServiceBackend();
  });
});

addMessageListener("socket-server-msg", function (msg) {
  mockWebSocket.then(socket => {
    socket.serverSendMsg(msg);
  });
});

var MockService = {
  requestID: 1,
  resolvers: new Map(),

  sendRequest(name, params) {
    return new Promise((resolve, reject) => {
      let id = this.requestID++;
      this.resolvers.set(id, { resolve, reject });
      sendAsyncMessage("service-request", {
        name: name,
        id: id,
        params: params,
      });
    });
  },

  handleResponse(response) {
    if (!this.resolvers.has(response.id)) {
      Cu.reportError(`Unexpected response for request ${response.id}`);
      return;
    }
    let resolver = this.resolvers.get(response.id);
    this.resolvers.delete(response.id);
    if (response.error) {
      resolver.reject(response.error);
    } else {
      resolver.resolve(response.result);
    }
  },

  init() {},

  register(pageRecord) {
    return this.sendRequest("register", pageRecord);
  },

  registration(pageRecord) {
    return this.sendRequest("registration", pageRecord);
  },

  unregister(pageRecord) {
    return this.sendRequest("unregister", pageRecord);
  },

  reportDeliveryError(messageId, reason) {
    sendAsyncMessage("service-delivery-error", {
      messageId: messageId,
      reason: reason,
    });
  },
};

addMessageListener("service-replace", function () {
  pushService.service = MockService;
});

addMessageListener("service-restore", function () {
  pushService.service = null;
});

addMessageListener("service-response", function (response) {
  MockService.handleResponse(response);
});
