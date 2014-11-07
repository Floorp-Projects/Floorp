"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
const { loadSubScript } = Cc['@mozilla.org/moz/jssubscript-loader;1'].
                          getService(Ci.mozIJSSubScriptLoader);

const EventUtils = {};
loadSubScript("chrome://marionette/content/EventUtils.js", EventUtils);

dump("Frame script loaded.\n");

addMessageListener("test:call", function (message) {
  dump("Calling function with name " + message.data + ".\n");

  XPCNativeWrapper.unwrap(content)[message.data]();
  sendAsyncMessage("test:call");
});

addMessageListener("test:click", function (message) {
  dump("Sending mouse click.\n");

  let target = message.objects.target;
  EventUtils.synthesizeMouseAtCenter(target, {},
                                     target.ownerDocument.defaultView);
});
