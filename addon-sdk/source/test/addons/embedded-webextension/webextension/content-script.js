browser.runtime.sendMessage("content script->sdk message", (reply) => {
  browser.runtime.sendMessage(reply);
});

let port = browser.runtime.connect();
port.onMessage.addListener((msg) => {
  port.postMessage(`content script received ${msg}`);
  port.disconnect();
});
port.postMessage("content script->sdk port message");
