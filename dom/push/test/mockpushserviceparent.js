/* eslint-env mozilla/frame-script */

"use strict";

/**
 * Defers one or more callbacks until the next turn of the event loop. Multiple
 * callbacks are executed in order.
 *
 * @param {Function[]} callbacks The callbacks to execute. One callback will be
 *  executed per tick.
 */
function waterfall(...callbacks) {
  callbacks
    .reduce(
      (promise, callback) =>
        promise.then(() => {
          callback();
        }),
      Promise.resolve()
    )
    .catch(Cu.reportError);
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

  QueryInterface: ChromeUtils.generateQI([Ci.nsIWebSocketChannel]),

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
    waterfall(
      () => this._listener.onMessageAvailable(this._context, msg),
      () => this._listener.onAcknowledge(this._context, 0)
    );
  },
};

var pushService = Cc["@mozilla.org/push/Service;1"].getService(
  Ci.nsIPushService
).wrappedJSObject;

var mockSocket;
var serverMsgs = [];

addMessageListener("socket-setup", function() {
  pushService.replaceServiceBackend({
    serverURI: "wss://push.example.org/",
    makeWebSocket(uri) {
      mockSocket = new MockWebSocketParent(uri);
      while (serverMsgs.length > 0) {
        let msg = serverMsgs.shift();
        mockSocket.serverSendMsg(msg);
      }
      return mockSocket;
    },
  });
});

addMessageListener("socket-teardown", function(msg) {
  pushService
    .restoreServiceBackend()
    .then(_ => {
      serverMsgs.length = 0;
      if (mockSocket) {
        mockSocket.close();
        mockSocket = null;
      }
      sendAsyncMessage("socket-server-teardown");
    })
    .catch(error => {
      Cu.reportError(`Error restoring service backend: ${error}`);
    });
});

addMessageListener("socket-server-msg", function(msg) {
  if (mockSocket) {
    mockSocket.serverSendMsg(msg);
  } else {
    serverMsgs.push(msg);
  }
});

var MockService = {
  requestID: 1,
  resolvers: new Map(),

  sendRequest(name, params) {
    return new Promise((resolve, reject) => {
      let id = this.requestID++;
      this.resolvers.set(id, { resolve, reject });
      sendAsyncMessage("service-request", {
        name,
        id,
        params,
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
      messageId,
      reason,
    });
  },

  uninit() {
    return Promise.resolve();
  },
};

async function replaceService(service) {
  await pushService.service.uninit();
  pushService.service = service;
  await pushService.service.init();
}

addMessageListener("service-replace", function() {
  replaceService(MockService)
    .then(_ => {
      sendAsyncMessage("service-replaced");
    })
    .catch(error => {
      Cu.reportError(`Error replacing service: ${error}`);
    });
});

addMessageListener("service-restore", function() {
  replaceService(null)
    .then(_ => {
      sendAsyncMessage("service-restored");
    })
    .catch(error => {
      Cu.reportError(`Error restoring service: ${error}`);
    });
});

addMessageListener("service-response", function(response) {
  MockService.handleResponse(response);
});
