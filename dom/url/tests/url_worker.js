/* eslint-env worker */

onmessage = function (event) {
  if (event.data != 0) {
    var worker = new Worker("url_worker.js");
    worker.onmessage = function (ev) {
      postMessage(ev.data);
    };

    worker.postMessage(event.data - 1);
    return;
  }

  let status = false;
  try {
    if (URL instanceof Object) {
      status = true;
    }
  } catch (e) {}

  postMessage({ type: "status", status, msg: "URL object:" + URL });

  status = false;
  var blob = null;
  try {
    blob = new Blob([]);
    status = true;
  } catch (e) {}

  postMessage({ type: "status", status, msg: "Blob:" + blob });

  status = false;
  let url = null;
  try {
    url = URL.createObjectURL(blob);
    status = true;
  } catch (e) {}

  postMessage({ type: "status", status, msg: "Blob URL:" + url });

  status = false;
  try {
    URL.revokeObjectURL(url);
    status = true;
  } catch (e) {}

  postMessage({ type: "status", status, msg: "Blob Revoke URL" });

  status = false;
  url = null;
  try {
    url = URL.createObjectURL(true);
  } catch (e) {
    status = true;
  }

  postMessage({
    type: "status",
    status,
    msg: "CreateObjectURL should fail if the arg is not a blob",
  });

  status = false;
  url = null;
  try {
    url = URL.createObjectURL(blob);
    status = true;
  } catch (e) {}

  postMessage({ type: "status", status, msg: "Blob URL2:" + url });
  postMessage({ type: "url", url });

  status = false;
  try {
    URL.createObjectURL({});
  } catch (e) {
    status = true;
  }

  postMessage({ type: "status", status, msg: "Exception wanted" });

  blob = new Blob([123]);
  var uri = URL.createObjectURL(blob);
  postMessage({
    type: "status",
    status: !!uri,
    msg: "The URI has been generated from the blob",
  });

  var u = new URL(uri);
  postMessage({
    type: "status",
    status: u.origin == location.origin,
    msg: "The URL generated from a blob URI has an origin.",
  });

  postMessage({ type: "finish" });
};
