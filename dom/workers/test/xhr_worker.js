/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
var xhr = new XMLHttpRequest();

function onload(event) {
  if (event.target != xhr) {
    throw "onload event.target != xhr";
  }

  if (event.target.status != 200) {
    var message = { type: "error",
                    error: event.target.status };
    postMessage(message);
  }

  var message = { type: "load",
                  data: xhr.responseText };
  postMessage(message);
}

xhr.onload = onload;
xhr.addEventListener("load", onload, false);
xhr.removeEventListener("load", onload, false);
if (!xhr.onload) {
  var message = { type: "error",
                  error: "Lost message listener!" };
  postMessage(message);
}

xhr.addEventListener("error", function(event) {
  if (event.target != xhr) {
    throw "onerror event.target != xhr";
  }
  var message = { type: "error",
                  error: event.target.status };
  postMessage(message);
}, false);

function onprogress(event) {
  if (event.target != xhr) {
    throw "onprogress event.target != xhr";
  }
  var message = { type: "progress",
                  current: event.loaded,
                  total: event.total };
  postMessage(message);
}
xhr.addEventListener("progress", onprogress, false);

xhr.addEventListener("foopety", function(event) {}, false);
xhr.removeEventListener("doopety", function(event) {}, false);

xhr.onloadend = function(event) {
  var message = { type: "loadend" };
  postMessage(message);  
}

var upload = xhr.upload;
upload.onprogress = function(event) { };
upload.addEventListener("foo", function(event) { }, false);
upload.removeEventListener("foo", function(event) { }, false);
upload.addEventListener("load", function(event) { }, false);
upload.removeEventListener("foo", function(event) { }, false);
upload.onload = function(event) {
  var message = { type: "upload.load" };
  postMessage(message);
}

onmessage = function(event) {
  if (xhr.DONE != 4 || XMLHttpRequest.DONE != 4) {
    throw "xhr constants not correct!";
  }
  if (xhr.readystate > xhr.UNSENT) {
    throw "XHR already running!";
  }
  xhr.open("POST", event.data);
  xhr.send("Data to send");
}
