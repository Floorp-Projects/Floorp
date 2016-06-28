(function (g) {
  "use strict";

  let url = SimpleTest.getTestFileURL("mockpushserviceparent.js");
  let chromeScript = SpecialPowers.loadChromeScript(url);

  /**
   * Replaces `PushService.jsm` with a mock implementation that handles requests
   * from the DOM API. This allows tests to simulate local errors and error
   * reporting, bypassing the `PushService.jsm` machinery.
   */
  function replacePushService(mockService) {
    chromeScript.sendSyncMessage("service-replace");
    chromeScript.addMessageListener("service-delivery-error", function(msg) {
      mockService.reportDeliveryError(msg.messageId, msg.reason);
    });
    chromeScript.addMessageListener("service-request", function(msg) {
      let promise;
      try {
        let handler = mockService[msg.name];
        promise = Promise.resolve(handler(msg.params));
      } catch (error) {
        promise = Promise.reject(error);
      }
      promise.then(result => {
        chromeScript.sendAsyncMessage("service-response", {
          id: msg.id,
          result: result,
        });
      }, error => {
        chromeScript.sendAsyncMessage("service-response", {
          id: msg.id,
          error: error,
        });
      });
    });
  }

  function restorePushService() {
    chromeScript.sendSyncMessage("service-restore");
  }

  let userAgentID = "8e1c93a9-139b-419c-b200-e715bb1e8ce8";

  let currentMockSocket = null;

  /**
   * Sets up a mock connection for the WebSocket backend. This only replaces
   * the transport layer; `PushService.jsm` still handles DOM API requests,
   * observes permission changes, writes to IndexedDB, and notifies service
   * workers of incoming push messages.
   */
  function setupMockPushSocket(mockWebSocket) {
    currentMockSocket = mockWebSocket;
    currentMockSocket._isActive = true;
    chromeScript.sendSyncMessage("socket-setup");
    chromeScript.addMessageListener("socket-client-msg", function(msg) {
      mockWebSocket.handleMessage(msg);
    });
  }

  function teardownMockPushSocket() {
    if (currentMockSocket) {
      return new Promise(resolve => {
        currentMockSocket._isActive = false;
        chromeScript.addMessageListener("socket-server-teardown", resolve);
        chromeScript.sendSyncMessage("socket-teardown");
      });
    }
    return Promise.resolve();
  }

  /**
   * Minimal implementation of web sockets for use in testing. Forwards
   * messages to a mock web socket in the parent process that is used
   * by the push service.
   */
  function MockWebSocket() {}

  let registerCount = 0;

  // Default implementation to make the push server work minimally.
  // Override methods to implement custom functionality.
  MockWebSocket.prototype = {
    // We only allow one active mock web socket to talk to the parent.
    // This flag is used to keep track of which mock web socket is active.
    _isActive: false,

    onHello(request) {
      this.serverSendMsg(JSON.stringify({
        messageType: "hello",
        uaid: userAgentID,
        status: 200,
        use_webpush: true,
      }));
    },

    onRegister(request) {
      this.serverSendMsg(JSON.stringify({
        messageType: "register",
        uaid: userAgentID,
        channelID: request.channelID,
        status: 200,
        pushEndpoint: "https://example.com/endpoint/" + registerCount++
      }));
    },

    onUnregister(request) {
      this.serverSendMsg(JSON.stringify({
        messageType: "unregister",
        channelID: request.channelID,
        status: 200,
      }));
    },

    onAck(request) {
      // Do nothing.
    },

    handleMessage(msg) {
      let request = JSON.parse(msg);
      let messageType = request.messageType;
      switch (messageType) {
      case "hello":
        this.onHello(request);
        break;
      case "register":
        this.onRegister(request);
        break;
      case "unregister":
        this.onUnregister(request);
        break;
      case "ack":
        this.onAck(request);
        break;
      default:
        throw new Error("Unexpected message: " + messageType);
      }
    },

    serverSendMsg(msg) {
      if (this._isActive) {
        chromeScript.sendAsyncMessage("socket-server-msg", msg);
      }
    },
  };

  g.MockWebSocket = MockWebSocket;
  g.setupMockPushSocket = setupMockPushSocket;
  g.teardownMockPushSocket = teardownMockPushSocket;
  g.replacePushService = replacePushService;
  g.restorePushService = restorePushService;
}(this));

// Remove permissions and prefs when the test finishes.
SimpleTest.registerCleanupFunction(() => {
  return new Promise(resolve =>
    SpecialPowers.flushPermissions(resolve)
  ).then(_ => SpecialPowers.flushPrefEnv()).then(_ => {
    restorePushService();
    return teardownMockPushSocket();
  });
});

function setPushPermission(allow) {
  return new Promise(resolve => {
    SpecialPowers.pushPermissions([
      { type: "desktop-notification", allow, context: document },
      ], resolve);
  });
}

function setupPrefs() {
  return SpecialPowers.pushPrefEnv({"set": [
    ["dom.push.enabled", true],
    ["dom.push.connection.enabled", true],
    ["dom.push.maxRecentMessageIDsPerSubscription", 0],
    ["dom.serviceWorkers.exemptFromPerDomainMax", true],
    ["dom.serviceWorkers.enabled", true],
    ["dom.serviceWorkers.testing.enabled", true]
    ]});
}

function setupPrefsAndReplaceService(mockService) {
  replacePushService(mockService);
  return setupPrefs();
}

function setupPrefsAndMockSocket(mockSocket) {
  setupMockPushSocket(mockSocket);
  return setupPrefs();
}

function injectControlledFrame(target = document.body) {
  return new Promise(function(res, rej) {
    var iframe = document.createElement("iframe");
    iframe.src = "/tests/dom/push/test/frame.html";

    var controlledFrame = {
      remove() {
        target.removeChild(iframe);
        iframe = null;
      },
      waitOnWorkerMessage(type) {
        return iframe ? iframe.contentWindow.waitOnWorkerMessage(type) :
               Promise.reject(new Error("Frame removed from document"));
      },
      innerWindowId() {
        var utils = SpecialPowers.getDOMWindowUtils(iframe.contentWindow);
        return utils.currentInnerWindowID;
      },
    };

    iframe.onload = () => res(controlledFrame);
    target.appendChild(iframe);
  });
}

function sendRequestToWorker(request) {
  return navigator.serviceWorker.ready.then(registration => {
    return new Promise((resolve, reject) => {
      var channel = new MessageChannel();
      channel.port1.onmessage = e => {
        (e.data.error ? reject : resolve)(e.data);
      };
      registration.active.postMessage(request, [channel.port2]);
    });
  });
}
