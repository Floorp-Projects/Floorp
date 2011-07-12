var events = [];

function runTest() {
  var xhr = new XMLHttpRequest();

  function pushEvent(event) {
    var str = event.type + "(" + xhr.readyState + ", '" + xhr.responseText +
              "'";
    if (typeof event.loaded != "undefined") {
      str += ", progressEvent";
    }
    str += ")";
    events.push(str);
  }

  xhr.onerror = function(event) {
    dump("Error: " + xhr.statusText + "\n");
  }

  xhr.onload = function(event) {
    pushEvent(event);
    postMessage(events);
  };

  xhr.onabort = function(event) {
    pushEvent(event);
    postMessage(events);
  };

  var count = 0;
  xhr.onreadystatechange = function(event) {
    pushEvent(event);
    if (++count == 3) {
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
  }
}

addEventListener("message", messageListener, false);
