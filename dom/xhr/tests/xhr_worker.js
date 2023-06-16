/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
var xhr = new XMLHttpRequest();

function onload(event) {
  if (event.target != xhr) {
    throw new Error("onload event.target != xhr");
  }

  if (event.target.status != 200) {
    const message = { type: "error", error: event.target.status };
    postMessage(message);
  }

  const message = { type: "load", data: xhr.responseText };
  postMessage(message);
}

xhr.onload = onload;
xhr.addEventListener("load", onload);
xhr.removeEventListener("load", onload);
if (!xhr.onload) {
  const message = { type: "error", error: "Lost message listener!" };
  postMessage(message);
}

xhr.onerror = function (event) {
  if (event.target != xhr) {
    throw new Error("onerror event.target != xhr");
  }
  var message = { type: "error", error: event.target.status };
  postMessage(message);
};
// eslint-disable-next-line no-self-assign
xhr.onerror = xhr.onerror;
// eslint-disable-next-line no-self-compare
if (!xhr.onerror || xhr.onerror != xhr.onerror) {
  throw new Error("onerror wasn't set properly");
}

function onprogress(event) {
  if (event.target != xhr) {
    throw new Error("onprogress event.target != xhr");
  }
  const message = {
    type: "progress",
    current: event.loaded,
    total: event.total,
  };
  postMessage(message);
}
xhr.addEventListener("progress", onprogress);

xhr.addEventListener("foopety", function (event) {});
xhr.removeEventListener("doopety", function (event) {});

xhr.onloadend = function (event) {
  const message = { type: "loadend" };
  postMessage(message);
};

var upload = xhr.upload;
upload.onprogress = function (event) {};
upload.addEventListener("foo", function (event) {});
upload.removeEventListener("foo", function (event) {});
upload.addEventListener("load", function (event) {});
upload.removeEventListener("foo", function (event) {});
upload.onload = function (event) {
  const message = { type: "upload.load" };
  postMessage(message);
};

onmessage = function (event) {
  if (xhr.DONE != 4 || XMLHttpRequest.DONE != 4) {
    throw new Error("xhr constants not correct!");
  }
  if (xhr.readystate > xhr.UNSENT) {
    throw new Error("XHR already running!");
  }
  xhr.open("POST", event.data);
  xhr.send("Data to send");
};
