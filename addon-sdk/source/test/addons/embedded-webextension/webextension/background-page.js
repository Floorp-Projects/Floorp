browser.runtime.sendMessage("bg->sdk message", (reply) => {
  browser.runtime.sendMessage(reply);
});

let port = browser.runtime.connect();
port.onMessage.addListener((msg) => {
  port.postMessage(`bg received ${msg}`);
  port.disconnect();
});
port.postMessage("bg->sdk port message");
