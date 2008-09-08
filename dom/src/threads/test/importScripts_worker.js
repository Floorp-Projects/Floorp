function messageListener(message, source) {
  switch (message) {
    case 'start':
      loadScripts("importScripts_worker_imported2.js");
      importedScriptFunction2();
      tryBadScripts();
      source.postMessage('started');
      break;
    case 'stop':
      tryBadScripts();
      postMessageToPool('stopped');
      break;
    default:
      throw new Error("Bad message: " + message);
      break;
  }
}

// This caused security exceptions in the past, make sure it doesn't!
var constructor = {}.constructor;

loadScripts("importScripts_worker_imported1.js");

// Try to call a function defined in the imported script.
importedScriptFunction();

function tryBadScripts() {
  var badScripts = [
    // Has a syntax error
    "importScripts_worker_imported3.js",
    // Throws an exception
    "importScripts_worker_imported4.js",
    // Shouldn't exist!
    "http://flippety.com/floppety/foo.js",
    // Not a valid url
    "http://flippety::foo_js ftw"
  ];

  for (var i = 0; i < badScripts.length; i++) {
    var caughtException = false;
    var url = badScripts[i];
    try {
      loadScripts(url);
    }
    catch (e) {
      caughtException = true;
    }
    if (!caughtException) {
      throw "Bad script didn't throw exception: " + url;
    }
  }
}

tryBadScripts();
