// Remove permissions and prefs when the test finishes.
SimpleTest.registerCleanupFunction(() =>
  new Promise(resolve => {
    SpecialPowers.flushPermissions(_ => {
      SpecialPowers.flushPrefEnv(resolve);
    });
  })
);

function setPushPermission(allow) {
  return new Promise(resolve => {
    SpecialPowers.pushPermissions([
      { type: "desktop-notification", allow, context: document },
      ], resolve);
  });
}

function setupPrefs() {
  return new Promise(resolve => {
    SpecialPowers.pushPrefEnv({"set": [
      ["dom.push.enabled", true],
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
