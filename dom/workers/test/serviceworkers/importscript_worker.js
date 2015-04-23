var counter = 0;
function callByScript() {
  ++counter;
}

importScripts(['importscript.sjs']);
importScripts(['importscript.sjs']);

onmessage = function(e) {
  self.clients.matchAll().then(function(res) {
    if (!res.length) {
      dump("ERROR: no clients are currently controlled.\n");
    }

    try {
      importScripts(['importscript.sjs']);
      res[0].postMessage("KO");
      return;
    } catch(e) {}

    res[0].postMessage(counter == 2 ? "OK" : "KO");
  });
};
