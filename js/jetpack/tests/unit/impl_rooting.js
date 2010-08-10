registerReceiver("ReceiveGCHandle", function(name, handle) {
  handle.onInvalidate = function() {
    sendMessage("onInvalidateReceived", handle.isValid);
  };
});
