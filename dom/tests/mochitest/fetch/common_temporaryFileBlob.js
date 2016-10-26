var data = new Array(256).join("1234567890ABCDEF");

function test_basic() {
  info("Simple test");

  fetch("/tests/dom/xhr/tests/temporaryFileBlob.sjs",
        { method: "POST", body: data })
  .then(response => {
    return response.blob();
  }).then(blob => {
    ok(blob instanceof Blob, "We have a blob!");
    is(blob.size, data.length, "Data length matches");

    var fr = new FileReader();
    fr.readAsText(blob);
    fr.onload = function() {
      is(fr.result, data, "Data content matches");
      next();
    }
  });
}

function test_worker() {
  info("XHR in workers");
  var w = new Worker('worker_temporaryFileBlob.js');
  w.onmessage = function(e) {
    if (e.data.type == 'info') {
      info(e.data.msg);
    } else if (e.data.type == 'check') {
      ok(e.data.what, e.data.msg);
    } else if (e.data.type == 'finish') {
      next();
    } else {
      ok(false, 'Something wrong happened');
    }
  }

  w.postMessage(42);
}
