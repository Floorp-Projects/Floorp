function testBlob(file, contents, testName) {
  // Load file using FileReader
  return new Promise(resolve => {
    let r = new FileReader();
    r.onload = event => {
      is(event.target.readyState, FileReader.DONE,
         "[FileReader] readyState in test FileReader.readAsBinaryString of " + testName);
      is(event.target.error, null,
         "[FileReader] no error in test FileReader.readAsBinaryString of " + testName);
      // Do not use |is(event.target.result, contents, "...");| that may output raw binary data.
      is(event.target.result.length, contents.length,
         "[FileReader] Length of result in test FileReader.readAsBinaryString of " + testName);
      ok(event.target.result == contents,
         "[FileReader] Content of result in test FileReader.readAsBinaryString of " + testName);
      is(event.lengthComputable, true,
         "[FileReader] lengthComputable in test FileReader.readAsBinaryString of " + testName);
      is(event.loaded, contents.length,
         "[FileReader] Loaded length in test FileReader.readAsBinaryString of " + testName);
      is(event.total, contents.length,
         "[FileReader] Total length in test FileReader.readAsBinaryString of " + testName);
      resolve();
    }
    r.readAsBinaryString(file);
  })

  // Load file using URL.createObjectURL and XMLHttpRequest
  .then(() => {
    return new Promise(resolve => {
      let xhr = new XMLHttpRequest();
      xhr.open("GET", URL.createObjectURL(file));
      xhr.onload = event => {
        XHRLoadHandler(event, resolve, contents, "XMLHttpRequest load of " + testName);
      };
      xhr.overrideMimeType('text/plain; charset=x-user-defined');
      xhr.send();
    });
  })

  // Send file to server using FormData and XMLHttpRequest
  .then(() => {
    return new Promise(resolve => {
      let xhr = new XMLHttpRequest();
      xhr.onload = function(event) {
        checkMPSubmission(JSON.parse(event.target.responseText),
                          [{ name: "hello", value: "world"},
                           { name: "myfile",
                             value: contents,
                             fileName: file.name || "blob",
                             contentType: file.type || "application/octet-stream" }]);
        resolve();
      }
      xhr.open("POST", "../../../dom/html/test/form_submit_server.sjs");

      let fd = new FormData;
      fd.append("hello", "world");
      fd.append("myfile", file);

      xhr.send(fd);
    });
  })

  // Send file to server using plain XMLHttpRequest
  .then(() => {
    return new Promise(resolve => {
      let xhr = new XMLHttpRequest();
      xhr.open("POST", "../../../dom/xhr/tests/file_XHRSendData.sjs");

      xhr.onload = function (event) {
        is(event.target.getResponseHeader("Result-Content-Type"),
           file.type ? file.type : null,
           "request content-type in XMLHttpRequest send of " + testName);
        is(event.target.getResponseHeader("Result-Content-Length"),
           String(file.size),
           "request content-length in XMLHttpRequest send of " + testName);
      };

      xhr.addEventListener("load", event => {
        XHRLoadHandler(event, resolve, contents, "XMLHttpRequest send of " + testName);
      });
      xhr.overrideMimeType('text/plain; charset=x-user-defined');
      xhr.send(file);
    });
  });
}

function convertXHRBinary(s) {
  let res = "";
  for (let i = 0; i < s.length; ++i) {
    res += String.fromCharCode(s.charCodeAt(i) & 255);
  }
  return res;
}

function XHRLoadHandler(event, resolve, contents, testName) {
  is(event.target.readyState, 4, "[XHR] readyState in test " + testName);
  is(event.target.status, 200, "[XHR] no error in test " + testName);
  // Do not use |is(convertXHRBinary(event.target.responseText), contents, "...");| that may output raw binary data.
  let convertedData = convertXHRBinary(event.target.responseText);
  is(convertedData.length, contents.length, "[XHR] Length of result in test " + testName);
  ok(convertedData == contents, "[XHR] Content of result in test " + testName);
  is(event.lengthComputable, event.total != 0, "[XHR] lengthComputable in test " + testName);
  is(event.loaded, contents.length, "[XHR] Loaded length in test " + testName);
  is(event.total, contents.length, "[XHR] Total length in test " + testName);
  resolve();
}

function checkMPSubmission(sub, expected) {
  function getPropCount(o) {
    let x, l = 0;
    for (x in o) ++l;
    return l;
  }

  is(sub.length, expected.length,
     "Correct number of items");
  let i;
  for (i = 0; i < expected.length; ++i) {
    if (!("fileName" in expected[i])) {
      is(sub[i].headers["Content-Disposition"],
         "form-data; name=\"" + expected[i].name + "\"",
         "Correct name (A)");
      is (getPropCount(sub[i].headers), 1,
          "Wrong number of headers (A)");
    }
    else {
      is(sub[i].headers["Content-Disposition"],
         "form-data; name=\"" + expected[i].name + "\"; filename=\"" +
           expected[i].fileName + "\"",
         "Correct name (B)");
      is(sub[i].headers["Content-Type"],
         expected[i].contentType,
         "Correct content type (B)");
      is (getPropCount(sub[i].headers), 2,
          "Wrong number of headers (B)");
    }
    // Do not use |is(sub[i].body, expected[i].value, "...");| that may output raw binary data.
    is(sub[i].body.length, expected[i].value.length,
       "Length of correct value");
    ok(sub[i].body == expected[i].value,
       "Content of correct value");
  }
}
