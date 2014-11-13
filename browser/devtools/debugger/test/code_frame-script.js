"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
const { loadSubScript } = Cc['@mozilla.org/moz/jssubscript-loader;1'].
                          getService(Ci.mozIJSSubScriptLoader);

const EventUtils = {};
loadSubScript("chrome://marionette/content/EventUtils.js", EventUtils);

dump("Frame script loaded.\n");

addMessageListener("test:call", function (message) {
  dump("Calling function with name " + message.data.name + ".\n");

  let data = message.data;
  XPCNativeWrapper.unwrap(content)[data.name].apply(undefined, data.args);
  sendAsyncMessage("test:call");
});

addMessageListener("test:click", function (message) {
  dump("Sending mouse click.\n");

  let target = message.objects.target;
  EventUtils.synthesizeMouseAtCenter(target, {},
                                     target.ownerDocument.defaultView);
});

addMessageListener("test:eval", function (message) {
  dump("Evalling string " + message.data.string + ".\n");

  content.eval(message.data.string);
  sendAsyncMessage("test:eval");
});
