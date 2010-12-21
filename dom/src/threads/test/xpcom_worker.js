var exception;
try {
  var xpcom = XPCOM;
}
catch(e) {
  exception = e;
}

if (!exception) {
  throw "Worker shouldn't be able to access the XPCOM object!";
}

onmessage = function(event) {
  if (event.data != "Hi") {
    throw "Bad message!";
  }
  postMessage("Done");
}
