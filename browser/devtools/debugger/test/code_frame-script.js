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
  let result = XPCNativeWrapper.unwrap(content)[data.name].apply(undefined, data.args);
  if (result && result.then) {
    result.then(() => {
      sendAsyncMessage("test:call");
    });
  } else {
    sendAsyncMessage("test:call");
  }
});

addMessageListener("test:click", function (message) {
  dump("Sending mouse click.\n");

  let target = message.objects.target;
  EventUtils.synthesizeMouseAtCenter(target, {},
                                     target.ownerDocument.defaultView);
});

addMessageListener("test:eval", function (message) {
  dump("Evalling string " + message.data.string + ".\n");

  let result = content.eval(message.data.string);
  if (result.then) {
    result.then(() => {
      sendAsyncMessage("test:eval");
    });
  } else {
    sendAsyncMessage("test:eval");
  }
});

let workers = {}

addMessageListener("test:createWorker", function (message) {
  dump("Creating worker with url '" + message.data.url + "'.\n");

  let url = message.data.url;
  let worker = new content.Worker(message.data.url);
  worker.addEventListener("message", function listener() {
    worker.removeEventListener("message", listener);
    sendAsyncMessage("test:createWorker");
  });
  workers[url] = worker;
});

addMessageListener("test:terminateWorker", function (message) {
  dump("Terminating worker with url '" + message.data.url + "'.\n");

  let url = message.data.url;
  workers[url].terminate();
  delete workers[url];
});
