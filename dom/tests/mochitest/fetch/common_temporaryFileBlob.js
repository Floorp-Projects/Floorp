var data = new Array(256).join("1234567890ABCDEF");

function test_fetch_basic() {
  info("Simple fetch test");

  fetch("/tests/dom/xhr/tests/temporaryFileBlob.sjs", {
    method: "POST",
    body: data,
  })
    .then(response => {
      return response.blob();
    })
    .then(blob => {
      ok(blob instanceof Blob, "We have a blob!");
      is(blob.size, data.length, "Data length matches");
      if ("SpecialPowers" in self) {
        is(
          SpecialPowers.wrap(blob).blobImplType,
          "StreamBlobImpl[TemporaryFileBlobImpl]",
          "We have a blob stored into a stream file"
        );
      }

      var fr = new FileReader();
      fr.readAsText(blob);
      fr.onload = function () {
        is(fr.result, data, "Data content matches");
        next();
      };
    });
}

function test_fetch_worker() {
  generic_worker_test("fetch in workers", "fetch");
}

function test_xhr_basic() {
  info("Simple XHR test");

  let xhr = new XMLHttpRequest();
  xhr.responseType = "blob";
  xhr.open("POST", "/tests/dom/xhr/tests/temporaryFileBlob.sjs");
  xhr.send(data);

  xhr.onreadystatechange = function () {
    if (xhr.readyState == 4) {
      let blob = xhr.response;

      ok(blob instanceof Blob, "We have a blob!");
      is(blob.size, data.length, "Data length matches");
      if ("SpecialPowers" in self) {
        is(
          SpecialPowers.wrap(blob).blobImplType,
          "StreamBlobImpl[TemporaryFileBlobImpl]",
          "We have a blob stored into a stream file"
        );
      }

      var fr = new FileReader();
      fr.readAsText(blob);
      fr.onload = function () {
        is(fr.result, data, "Data content matches");
        next();
      };
    }
  };
}

function test_xhr_worker() {
  generic_worker_test("XHR in workers", "xhr");
}

function test_response_basic() {
  info("Response");

  let r = new Response(data);
  r.blob().then(blob => {
    ok(blob instanceof Blob, "We have a blob!");
    is(blob.size, data.length, "Data length matches");
    if ("SpecialPowers" in self) {
      is(
        SpecialPowers.wrap(blob).blobImplType,
        "StreamBlobImpl[TemporaryFileBlobImpl]",
        "We have a blob stored into a stream file"
      );
    }

    var fr = new FileReader();
    fr.readAsText(blob);
    fr.onload = function () {
      is(fr.result, data, "Data content matches");
      next();
    };
  });
}

function test_response_worker() {
  generic_worker_test("Response in workers", "response");
}

function test_request_basic() {
  info("Request");

  let r = new Request("https://example.com", { body: data, method: "POST" });
  r.blob().then(blob => {
    ok(blob instanceof Blob, "We have a blob!");
    is(blob.size, data.length, "Data length matches");
    if ("SpecialPowers" in self) {
      is(
        SpecialPowers.wrap(blob).blobImplType,
        "StreamBlobImpl[TemporaryFileBlobImpl]",
        "We have a blob stored into a stream file"
      );
    }

    var fr = new FileReader();
    fr.readAsText(blob);
    fr.onload = function () {
      is(fr.result, data, "Data content matches");
      next();
    };
  });
}

function test_request_worker() {
  generic_worker_test("Request in workers", "request");
}

function generic_worker_test(title, what) {
  info(title);

  var w = new Worker("worker_temporaryFileBlob.js");
  w.onmessage = function (e) {
    if (e.data.type == "info") {
      info(e.data.msg);
    } else if (e.data.type == "check") {
      ok(e.data.what, e.data.msg);
    } else if (e.data.type == "finish") {
      next();
    } else {
      ok(false, "Something wrong happened");
    }
  };

  w.postMessage(what);
}
