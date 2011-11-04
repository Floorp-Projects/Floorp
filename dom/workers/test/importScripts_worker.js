/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
// Try no args. This shouldn't do anything.
importScripts();

// This caused security exceptions in the past, make sure it doesn't!
var constructor = {}.constructor;

importScripts("importScripts_worker_imported1.js");

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
      importScripts(url);
    }
    catch (e) {
      caughtException = true;
    }
    if (!caughtException) {
      throw "Bad script didn't throw exception: " + url;
    }
  }
}

const url = "data:text/plain,const startResponse = 'started';";
importScripts(url);

onmessage = function(event) {
  switch (event.data) {
    case 'start':
      importScripts("importScripts_worker_imported2.js");
      importedScriptFunction2();
      tryBadScripts();
      postMessage(startResponse);
      break;
    case 'stop':
      tryBadScripts();
      postMessage('stopped');
      break;
    default:
      throw new Error("Bad message: " + event.data);
      break;
  }
}

tryBadScripts();
