var xhr = new XMLHttpRequest();

function onload(event) {
  if (event.target != xhr) {
    throw "onload event.target != xhr";
  }

  if (event.target.status != 200) {
    var message = { type: "error",
                    error: event.target.status };
    postMessage(message.toSource());
  }

  var message = { type: "load",
                  data: xhr.responseText };
  postMessage(message.toSource());
}

xhr.onload = onload;
xhr.addEventListener("load", onload, false);
xhr.removeEventListener("load", onload, false);
if (!xhr.onload) {
  var message = { type: "error",
                  error: "Lost message listener!" };
  postMessage(message.toSource());
}

xhr.addEventListener("error", function(event) {
  if (event.target != xhr) {
    throw "onerror event.target != xhr";
  }
  var message = { type: "error",
                  error: event.target.status };
  postMessage(message.toSource());
}, false);

function onprogress(event) {
  if (event.target != xhr) {
    throw "onprogress event.target != xhr";
  }
  var message = { type: "progress",
                  current: event.loaded,
                  total: event.total };
  postMessage(message.toSource());
}
xhr.addEventListener("progress", onprogress, false);

xhr.addEventListener("foopety", function(event) {}, false);
xhr.removeEventListener("doopety", function(event) {}, false);

xhr.onloadend = function(event) {
  var message = { type: "loadend" };
  postMessage(message.toSource());  
}

var upload = xhr.upload;
upload.onprogress = function(event) { };
upload.addEventListener("readystatechange", function(event) { }, false);
upload.removeEventListener("readystatechange", function(event) { }, false);
upload.addEventListener("load", function(event) { }, false);
upload.removeEventListener("readystatechange", function(event) { }, false);

onmessage = function(event) {
  if (xhr.readystate > 0) {
    throw "XHR already running!";
  }
  xhr.open("GET", event.data);
  xhr.send(null);
}
