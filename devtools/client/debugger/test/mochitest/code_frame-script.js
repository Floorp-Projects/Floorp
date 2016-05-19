"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;
const { loadSubScript } = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                          getService(Ci.mozIJSSubScriptLoader);

// Set up a dummy environment so that EventUtils works. We need to be careful to
// pass a window object into each EventUtils method we call rather than having
// it rely on the |window| global.
let EventUtils = {};
EventUtils.window = content;
EventUtils.parent = EventUtils.window;
EventUtils._EU_Ci = Components.interfaces;
EventUtils._EU_Cc = Components.classes;
EventUtils.navigator = content.navigator;
EventUtils.KeyboardEvent = content.KeyboardEvent;
loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

dump("Frame script loaded.\n");

var workers = {};

this.call = function (name, args) {
  dump("Calling function with name " + name + ".\n");

  dump("args " + JSON.stringify(args) + "\n");
  return XPCNativeWrapper.unwrap(content)[name].apply(undefined, args);
};

this._eval = function (string) {
  dump("Evalling string.\n");

  return content.eval(string);
};

this.generateMouseClick = function (path) {
  dump("Generating mouse click.\n");

  let target = eval(path);
  EventUtils.synthesizeMouseAtCenter(target, {},
                                     target.ownerDocument.defaultView);
};

this.createWorker = function (url) {
  dump("Creating worker with url '" + url + "'.\n");

  return new Promise(function (resolve, reject) {
    let worker = new content.Worker(url);
    worker.addEventListener("message", function listener() {
      worker.removeEventListener("message", listener);
      workers[url] = worker;
      resolve();
    });
  });
};

this.terminateWorker = function (url) {
  dump("Terminating worker with url '" + url + "'.\n");

  workers[url].terminate();
  delete workers[url];
};

this.postMessageToWorker = function (url, message) {
  dump("Posting message to worker with url '" + url + "'.\n");

  return new Promise(function (resolve) {
    let worker = workers[url];
    worker.postMessage(message);
    worker.addEventListener("message", function listener() {
      worker.removeEventListener("message", listener);
      resolve();
    });
  });
};

addMessageListener("jsonrpc", function ({ data: { method, params, id } }) {
  method = this[method];
  Promise.resolve().then(function () {
    return method.apply(undefined, params);
  }).then(function (result) {
    sendAsyncMessage("jsonrpc", {
      result: result,
      error: null,
      id: id
    });
  }, function (error) {
    sendAsyncMessage("jsonrpc", {
      result: null,
      error: error.message.toString(),
      id: id
    });
  });
});

addMessageListener("test:postMessageToWorker", function (message) {
  dump("Posting message '" + message.data.message + "' to worker with url '" +
       message.data.url + "'.\n");

  let worker = workers[message.data.url];
  worker.postMessage(message.data.message);
  worker.addEventListener("message", function listener() {
    worker.removeEventListener("message", listener);
    sendAsyncMessage("test:postMessageToWorker");
  });
});
