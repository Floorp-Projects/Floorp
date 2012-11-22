/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
function runTest() {
  var xhr = new XMLHttpRequest();

  var events = [];
  function pushEvent(event) {
    var readyState, responseText, status, statusText;

    try {
      readyState = xhr.readyState;
    }
    catch (e) {
      readyState = "[exception]";
    }

    try {
      responseText = xhr.responseText;
    }
    catch (e) {
      responseText = "[exception]";
    }

    try {
      status = xhr.status;
    }
    catch (e) {
      status = "[exception]";
    }

    try {
      statusText = xhr.statusText;
    }
    catch (e) {
      statusText = "[exception]";
    }

    var str = event.type + "(" + readyState + ", '" + responseText + "', " +
              status + ", '" + statusText + "'";
    if (event instanceof ProgressEvent) {
      str += ", progressEvent";
    }
    str += ")";

    events.push(str);
  }

  xhr.onerror = function(event) {
    throw new Error("Error: " + xhr.statusText);
  }

  xhr.onload = function(event) {
    throw new Error("Shouldn't have gotten load event!");
  };

  var seenAbort;
  xhr.onabort = function(event) {
    if (seenAbort) {
      throw new Error("Already seen the abort event!");
    }
    seenAbort = true;

    pushEvent(event);
    postMessage(events);
  };

  xhr.onreadystatechange = function(event) {
    pushEvent(event);
    if (xhr.readyState == xhr.HEADERS_RECEIVED) {
      xhr.abort();
    }
  };

  xhr.open("GET", "testXHR.txt");
  xhr.overrideMimeType("text/plain");
  xhr.send(null);
}

function messageListener(event) {
  switch (event.data) {
    case "start":
      runTest();
      break;
    default:
      throw new Error("Bad message!");
  }
}

addEventListener("message", messageListener, false);
