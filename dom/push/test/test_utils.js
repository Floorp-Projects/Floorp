"use strict";

const url = SimpleTest.getTestFileURL("mockpushserviceparent.js");
const chromeScript = SpecialPowers.loadChromeScript(url);

/**
 * Replaces `PushService.jsm` with a mock implementation that handles requests
 * from the DOM API. This allows tests to simulate local errors and error
 * reporting, bypassing the `PushService.jsm` machinery.
 */
async function replacePushService(mockService) {
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
        result,
      });
    }, error => {
      chromeScript.sendAsyncMessage("service-response", {
        id: msg.id,
        error,
      });
    });
  });
  await new Promise(resolve => {
    chromeScript.addMessageListener("service-replaced", function onReplaced() {
      chromeScript.removeMessageListener("service-replaced", onReplaced);
      resolve();
    });
    chromeScript.sendAsyncMessage("service-replace");
  });
}

async function restorePushService() {
  await new Promise(resolve => {
    chromeScript.addMessageListener("service-restored", function onRestored() {
      chromeScript.removeMessageListener("service-restored", onRestored);
      resolve();
    });
    chromeScript.sendAsyncMessage("service-restore");
  });
}

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
class MockWebSocket {
  // Default implementation to make the push server work minimally.
  // Override methods to implement custom functionality.
  constructor() {
    this.userAgentID = "8e1c93a9-139b-419c-b200-e715bb1e8ce8";
    this.registerCount = 0;
    // We only allow one active mock web socket to talk to the parent.
    // This flag is used to keep track of which mock web socket is active.
    this._isActive = false;
  }

  onHello(request) {
    this.serverSendMsg(JSON.stringify({
      messageType: "hello",
      uaid: this.userAgentID,
      status: 200,
      use_webpush: true,
    }));
  }

  onRegister(request) {
    this.serverSendMsg(JSON.stringify({
      messageType: "register",
      uaid: this.userAgentID,
      channelID: request.channelID,
      status: 200,
      pushEndpoint: "https://example.com/endpoint/" + this.registerCount++,
    }));
  }

  onUnregister(request) {
    this.serverSendMsg(JSON.stringify({
      messageType: "unregister",
      channelID: request.channelID,
      status: 200,
    }));
  }

  onAck(request) {
    // Do nothing.
  }

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
  }

  serverSendMsg(msg) {
    if (this._isActive) {
      chromeScript.sendAsyncMessage("socket-server-msg", msg);
    }
  }
}

// Remove permissions and prefs when the test finishes.
SimpleTest.registerCleanupFunction(async function() {
  await new Promise(resolve => SpecialPowers.flushPermissions(resolve));
  await SpecialPowers.flushPrefEnv();
  await restorePushService();
  await teardownMockPushSocket();
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
    ["dom.serviceWorkers.testing.enabled", true],
    ]});
}

async function setupPrefsAndReplaceService(mockService) {
  await replacePushService(mockService);
  await setupPrefs();
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

function waitForActive(swr) {
  let sw = swr.installing || swr.waiting || swr.active;
  return new Promise(resolve => {
    if (sw.state === "activated") {
      resolve(swr);
      return;
    }
    sw.addEventListener("statechange", function onStateChange(evt) {
      if (sw.state === "activated") {
        sw.removeEventListener("statechange", onStateChange);
        resolve(swr);
      }
    });
  });
}

function base64UrlDecode(s) {
  s = s.replace(/-/g, "+").replace(/_/g, "/");

  // Replace padding if it was stripped by the sender.
  // See http://tools.ietf.org/html/rfc4648#section-4
  switch (s.length % 4) {
    case 0:
      break; // No pad chars in this case
    case 2:
      s += "==";
      break; // Two pad chars
    case 3:
      s += "=";
      break; // One pad char
    default:
      throw new Error("Illegal base64url string!");
  }

  // With correct padding restored, apply the standard base64 decoder
  var decoded = atob(s);

  var array = new Uint8Array(new ArrayBuffer(decoded.length));
  for (var i = 0; i < decoded.length; i++) {
    array[i] = decoded.charCodeAt(i);
  }
  return array;
}
