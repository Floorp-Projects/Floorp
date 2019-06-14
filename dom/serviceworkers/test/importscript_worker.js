var counter = 0;
function callByScript() {
  ++counter;
}

// Use multiple scripts in this load to verify we support that case correctly.
// See bug 1249351 for a case where we broke this.
importScripts('lorem_script.js', 'importscript.sjs');

importScripts('importscript.sjs');

var missingScriptFailed = false;
try {
  importScripts(['there-is-nothing-here.js']);
} catch(e) {
  missingScriptFailed = true;
}

onmessage = function(e) {
  self.clients.matchAll().then(function(res) {
    if (!res.length) {
      dump("ERROR: no clients are currently controlled.\n");
    }

    if (!missingScriptFailed) {
      res[0].postMessage("KO");
    }

    try {
      // new unique script should fail
      importScripts(['importscript.sjs?unique=true']);
      res[0].postMessage("KO");
      return;
    } catch(ex) {}

    try {
      // duplicate script previously offlined should succeed
      importScripts(['importscript.sjs']);
    } catch(ex) {
      res[0].postMessage("KO");
      return;
    }

    res[0].postMessage(counter == 3 ? "OK" : "KO");
  });
};
