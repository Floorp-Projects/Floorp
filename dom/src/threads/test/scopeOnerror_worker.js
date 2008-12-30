if (onerror !== undefined || typeof(onerror) != "undefined") {
  throw "Undefined onerror has bad type!";
}

onerror = function(event) {
  if (!event.cancelable) {
    throw "Error event is not cancelable!";
  }

  if (event.target != this) {
    throw "Error event is targeted at the wrong object!";
  }

  if (event.message == "uncaught exception: This error should not make it back out") {
    event.preventDefault();
    setTimeout("postMessage('Done');", 10);
  }
}

onmessage = function(event) {
  if (onerror === undefined || typeof(onerror) == "undefined") {
    throw "onerror has a bad type!";
  }
  throw event.data;
}
