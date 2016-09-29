var data = new Array(256).join("1234567890ABCDEF");

function createXHR() {
  var xhr = new XMLHttpRequest();
  xhr.open("POST", "temporaryFileBlob.sjs");
  xhr.responseType = 'blob';
  xhr.send({toString: function() { return data; }});
  return xhr;
}

function test_simple() {
  info("Simple test");

  var xhr = createXHR();

  xhr.onloadend = function() {
    ok(xhr.response instanceof Blob, "We have a blob!");
    is(xhr.response.size, data.length, "Data length matches");

    var fr = new FileReader();
    fr.readAsText(xhr.response);
    fr.onload = function() {
      is(fr.result, data, "Data content matches");
      next();
    }
  }
}

function test_abort() {
  info("Aborting during onloading");

  var xhr = createXHR();

  xhr.onprogress = function() {
    xhr.abort();
  }

  xhr.onloadend = function() {
    ok(!xhr.response, "We should not have a Blob!");
    next();
  }
}

function test_reuse() {
  info("Reuse test");

  var xhr = createXHR();

  var count = 0;
  xhr.onloadend = function() {
    ok(xhr.response instanceof Blob, "We have a blob!");
    is(xhr.response.size, data.length, "Data length matches");

    var fr = new FileReader();
    fr.readAsText(xhr.response);
    fr.onload = function() {
      is(fr.result, data, "Data content matches");
      if (++count > 2) {
        next();
        return;
      }

      xhr.open("POST", "temporaryFileBlob.sjs");
      xhr.responseType = 'blob';
      xhr.send({toString: function() { return data; }});
    }
  }
}

function test_worker_generic(test) {
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

  w.postMessage(test);
}

function test_worker() {
  info("XHR in workers");
  test_worker_generic('simple');
}

function test_worker_abort() {
  info("XHR in workers");
  test_worker_generic('abort');
}

function test_worker_reuse() {
  info("XHR in workers");
  test_worker_generic('reuse');
}
