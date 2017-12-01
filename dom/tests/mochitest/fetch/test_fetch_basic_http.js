var path = "/tests/dom/xhr/tests/";

var passFiles = [['file_XHR_pass1.xml', 'GET', 200, 'OK', 'text/xml'],
                 ['file_XHR_pass2.txt', 'GET', 200, 'OK', 'text/plain'],
                 ['file_XHR_pass3.txt', 'GET', 200, 'OK', 'text/plain'],
                 ];

function testURL() {
  var promises = [];
  passFiles.forEach(function(entry) {
    var p = fetch(path + entry[0]).then(function(res) {
      ok(res.type !== "error", "Response should not be an error for " + entry[0]);
      is(res.status, entry[2], "Status should match expected for " + entry[0]);
      is(res.statusText, entry[3], "Status text should match expected for " + entry[0]);
      if (entry[0] != "file_XHR_pass3.txt")
        ok(res.url.endsWith(path + entry[0]), "Response url should match request for simple fetch for " + entry[0]);
      else
        ok(res.url.endsWith(path + "file_XHR_pass2.txt"), "Response url should match request for simple fetch for " + entry[0]);
      is(res.headers.get('content-type'), entry[4], "Response should have content-type for " + entry[0]);
    });
    promises.push(p);
  });

  return Promise.all(promises);
}

var failFiles = [['ftp://localhost' + path + 'file_XHR_pass1.xml', 'GET']];

function testURLFail() {
  var promises = [];
  failFiles.forEach(function(entry) {
    var p = fetch(entry[0]).then(function(res) {
      ok(false, "Response should be an error for " + entry[0]);
    }, function(e) {
      ok(e instanceof TypeError, "Response should be an error for " + entry[0]);
    });
    promises.push(p);
  });

  return Promise.all(promises);
}

function testRequestGET() {
  var promises = [];
  passFiles.forEach(function(entry) {
    var req = new Request(path + entry[0], { method: entry[1] });
    var p = fetch(req).then(function(res) {
      ok(res.type !== "error", "Response should not be an error for " + entry[0]);
      is(res.status, entry[2], "Status should match expected for " + entry[0]);
      is(res.statusText, entry[3], "Status text should match expected for " + entry[0]);
      if (entry[0] != "file_XHR_pass3.txt")
        ok(res.url.endsWith(path + entry[0]), "Response url should match request for simple fetch for " + entry[0]);
      else
        ok(res.url.endsWith(path + "file_XHR_pass2.txt"), "Response url should match request for simple fetch for " + entry[0]);
      is(res.headers.get('content-type'), entry[4], "Response should have content-type for " + entry[0]);
    });
    promises.push(p);
  });

  return Promise.all(promises);
}

function arraybuffer_equals_to(ab, s) {
  is(ab.byteLength, s.length, "arraybuffer byteLength should match");

  var u8v = new Uint8Array(ab);
  is(String.fromCharCode.apply(String, u8v), s, "arraybuffer bytes should match");
}

function testResponses() {
  var fetches = [
    fetch(path + 'file_XHR_pass2.txt').then((res) => {
      is(res.status, 200, "status should match");
      return res.text().then((v) => is(v, "hello pass\n", "response should match"));
    }),

    fetch(path + 'file_XHR_binary1.bin').then((res) => {
      is(res.status, 200, "status should match");
      return res.arrayBuffer().then((v) =>
        arraybuffer_equals_to(v, "\xaa\xee\0\x03\xff\xff\xff\xff\xbb\xbb\xbb\xbb")
      )
    }),

    new Promise((resolve, reject) => {
      var jsonBody = JSON.stringify({title: "aBook", author: "john"});
      var req = new Request(path + 'responseIdentical.sjs', {
                              method: 'POST',
                              body: jsonBody,
                            });
      var p = fetch(req).then((res) => {
        is(res.status, 200, "status should match");
        return res.json().then((v) => {
          is(JSON.stringify(v), jsonBody, "json response should match");
        });
      });
      resolve(p);
    }),

    new Promise((resolve, reject) => {
      var req = new Request(path + 'responseIdentical.sjs', {
                              method: 'POST',
                              body: '{',
                            });
      var p = fetch(req).then((res) => {
        is(res.status, 200, "wrong status");
        return res.json().then(
          (v) => ok(false, "expected json parse failure"),
          (e) => ok(true, "expected json parse failure")
        );
      });
      resolve(p);
    }),
  ];

  return Promise.all(fetches);
}

function testBlob() {
  return fetch(path + '/file_XHR_binary2.bin').then((r) => {
    ok(r.status, 200, "status should match");
    return r.blob().then((b) => {
      ok(b.size, 65536, "blob should have size 65536");
      return readAsArrayBuffer(b).then(function(ab) {
        var u8 = new Uint8Array(ab);
        for (var i = 0; i < 65536; i++) {
          if (u8[i] !== (i & 255)) {
            break;
          }
        }
        is(i, 65536, "wrong value at offset " + i);
      });
    });
  });
}

// This test is a copy of dom/html/test/formData_test.js testSend() modified to
// use the fetch API. Please change this if you change that.
function testFormDataSend() {
  var file, blob = new Blob(['hey'], {type: 'text/plain'});

  var fd = new FormData();
  fd.append("string", "hey");
  fd.append("empty", blob);
  fd.append("explicit", blob, "explicit-file-name");
  fd.append("explicit-empty", blob, "");
  file = new File([blob], 'testname',  {type: 'text/plain'});
  fd.append("file-name", file);
  file = new File([blob], '',  {type: 'text/plain'});
  fd.append("empty-file-name", file);
  file = new File([blob], 'testname',  {type: 'text/plain'});
  fd.append("file-name-overwrite", file, "overwrite");

  var req = new Request("/tests/dom/html/test/form_submit_server.sjs", {
                          method: 'POST',
                          body: fd,
                        });

  return fetch(req).then((r) => {
    ok(r.status, 200, "status should match");
    return r.json().then((response) => {
      for (var entry of response) {
        if (entry.headers['Content-Disposition'] != 'form-data; name="string"') {
          is(entry.headers['Content-Type'], 'text/plain');
        }

        is(entry.body, 'hey');
      }

      is(response[1].headers['Content-Disposition'],
          'form-data; name="empty"; filename="blob"');

      is(response[2].headers['Content-Disposition'],
          'form-data; name="explicit"; filename="explicit-file-name"');

      is(response[3].headers['Content-Disposition'],
          'form-data; name="explicit-empty"; filename=""');

      is(response[4].headers['Content-Disposition'],
          'form-data; name="file-name"; filename="testname"');

      is(response[5].headers['Content-Disposition'],
          'form-data; name="empty-file-name"; filename=""');

      is(response[6].headers['Content-Disposition'],
          'form-data; name="file-name-overwrite"; filename="overwrite"');
    });
  });
}

function runTest() {
  return Promise.resolve()
    .then(testURL)
    .then(testURLFail)
    .then(testRequestGET)
    .then(testResponses)
    .then(testBlob)
    .then(testFormDataSend)
    // Put more promise based tests here.
}
