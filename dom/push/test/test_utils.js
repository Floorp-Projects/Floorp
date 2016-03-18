(function (g) {
  "use strict";

  let url = SimpleTest.getTestFileURL("mockpushserviceparent.js");
  let chromeScript = SpecialPowers.loadChromeScript(url);

  let userAgentID = "8e1c93a9-139b-419c-b200-e715bb1e8ce8";

  let currentMockSocket = null;

  function setupMockPushService(mockWebSocket) {
    currentMockSocket = mockWebSocket;
    currentMockSocket._isActive = true;
    chromeScript.sendSyncMessage("setup");
    chromeScript.addMessageListener("client-msg", function(msg) {
      mockWebSocket.handleMessage(msg);
    });
  }

  function teardownMockPushService() {
    if (currentMockSocket) {
      currentMockSocket._isActive = false;
      chromeScript.sendSyncMessage("teardown");
    }
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
      // Do nothing.
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
        chromeScript.sendAsyncMessage("server-msg", msg);
      }
    },
  };

  g.MockWebSocket = MockWebSocket;
  g.setupMockPushService = setupMockPushService;
  g.teardownMockPushService = teardownMockPushService;
}(this));

// Remove permissions and prefs when the test finishes.
SimpleTest.registerCleanupFunction(() => {
  new Promise(resolve => {
    SpecialPowers.flushPermissions(_ => {
      SpecialPowers.flushPrefEnv(resolve);
    });
  }).then(_ => {
    teardownMockPushService();
  });
});

function setPushPermission(allow) {
  return new Promise(resolve => {
    SpecialPowers.pushPermissions([
      { type: "desktop-notification", allow, context: document },
      ], resolve);
  });
}

function setupPrefsAndMock(mockSocket) {
  return new Promise(resolve => {
    setupMockPushService(mockSocket);
    SpecialPowers.pushPrefEnv({"set": [
      ["dom.push.enabled", true],
      ["dom.push.connection.enabled", true],
      ["dom.serviceWorkers.exemptFromPerDomainMax", true],
      ["dom.serviceWorkers.enabled", true],
      ["dom.serviceWorkers.testing.enabled", true]
      ]}, resolve);
  });
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
