onmessage = function() {
  var exists = true;
  try {
    var bc = new BroadcastChannel('foobar');
  } catch(e) {
    exists = false;
  }

  postMessage({ exists: exists });
}

