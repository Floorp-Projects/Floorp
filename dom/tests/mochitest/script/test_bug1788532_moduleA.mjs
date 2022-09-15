let xhrFinished = false;

import("./test_bug1788532_moduleB.mjs")
  .then(() => {
    ok(xhrFinished, "Loaded after XHR finished");
    SimpleTest.finish();
  })
  .catch(e => {
    ok(false, "Caught exception: " + e);
  });

let request = new XMLHttpRequest();
request.open("GET", "./slow.sjs", false);
request.send(null);
xhrFinished = true;
