"use strict";

dump("Frame script loaded.\n");

addMessageListener("test:call", function (message) {
  dump("Calling function with name " + message.data + ".\n");

  XPCNativeWrapper.unwrap(content)[message.data]();
});
