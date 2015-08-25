window.addEventListener("load", function() {
  addon.port.emit("load", "ok");
});

addon.postMessage("first message");
addon.on("message", function(msg) {
  if (msg == "ping")
    addon.postMessage("pong");
});

addon.port.on("ping", function() {
  addon.port.emit("pong");
});
