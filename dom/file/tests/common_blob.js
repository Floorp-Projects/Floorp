const RANGE_1 = 1;
const RANGE_2 = 2;

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

function testSlice(file, size, type, contents, fileType, range) {
  is(file.type, type, fileType + " file is correct type");
  is(file.size, size, fileType + " file is correct size");
  ok(file instanceof File, fileType + " file is a File");
  ok(file instanceof Blob, fileType + " file is also a Blob");

  let slice = file.slice(0, size);
  ok(slice instanceof Blob, fileType + " fullsize slice is a Blob");
  ok(!(slice instanceof File), fileType + " fullsize slice is not a File");

  slice = file.slice(0, 1234);
  ok(slice instanceof Blob, fileType + " sized slice is a Blob");
  ok(!(slice instanceof File), fileType + " sized slice is not a File");

  slice = file.slice(0, size, "foo/bar");
  is(slice.type, "foo/bar", fileType + " fullsize slice foo/bar type");

  slice = file.slice(0, 5432, "foo/bar");
  is(slice.type, "foo/bar", fileType + " sized slice foo/bar type");

  is(slice.slice(0, 10).type, "", fileType + " slice-slice type");
  is(slice.slice(0, 10).size, 10, fileType + " slice-slice size");
  is(slice.slice(0, 10, "hello/world").type, "hello/world", fileType + " slice-slice hello/world type");
  is(slice.slice(0, 10, "hello/world").size, 10, fileType + " slice-slice hello/world size");

  // Start, end, expected size
  var indexes_range_1 = [[0, size, size],
                         [0, 1234, 1234],
                         [size-500, size, 500],
                         [size-500, size+500, 500],
                         [size+500, size+1500, 0],
                         [0, 0, 0],
                         [1000, 1000, 0],
                         [size, size, 0],
                         [undefined, undefined, size],
                         [0, undefined, size],
                        ];

  var indexes_range_2 = [[100, undefined, size-100],
                         [-100, undefined, 100],
                         [100, -100, size-200],
                         [-size-100, undefined, size],
                         [-2*size-100, 500, 500],
                         [0, -size-100, 0],
                         [100, -size-100, 0],
                         [50, -size+100, 50],
                         [0, 33000, 33000],
                         [1000, 34000, 33000],
                        ];

  let indexes;
  if (range == RANGE_1) {
    indexes = indexes_range_1;
  } else if (range == RANGE_2) {
    indexes = indexes_range_2;
  } else {
    throw "Invalid range!"
  }

  function runNextTest() {
    if (indexes.length == 0) {
      return Promise.resolve(true);
    }

    let index = indexes.shift();

    let sliceContents;
    let testName;
    if (index[0] == undefined) {
      slice = file.slice();
      sliceContents = contents.slice();
      testName = fileType + " slice()";
    } else if (index[1] == undefined) {
      slice = file.slice(index[0]);
      sliceContents = contents.slice(index[0]);
      testName = fileType + " slice(" + index[0] + ")";
    } else {
      slice = file.slice(index[0], index[1]);
      sliceContents = contents.slice(index[0], index[1]);
      testName = fileType + " slice(" + index[0] + ", " + index[1] + ")";
    }

    is(slice.type, "", testName + " type");
    is(slice.size, index[2], testName + " size");
    is(sliceContents.length, index[2], testName + " data size");

    return testBlob(slice, sliceContents, testName).then(runNextTest);
  }

  return runNextTest()
  .then(() => {
    // Slice of slice
    let sliceOfSlice = file.slice(0, 40000);
    return testBlob(sliceOfSlice.slice(5000, 42000), contents.slice(5000, 40000), "file slice slice");
  })
  .then(() => {
    // ...of slice of slice
    let sliceOfSlice = file.slice(0, 40000).slice(5000, 42000).slice(400, 700);
    SpecialPowers.gc();
    return testBlob(sliceOfSlice, contents.slice(5400, 5700), "file slice slice slice");
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

function createCanvasURL() {
  return new Promise(resolve => {
    // Create a decent-sized image
    let cx = $("canvas").getContext('2d');
    let s = cx.canvas.width;
    let grad = cx.createLinearGradient(0, 0, s-1, s-1);
    for (i = 0; i < 0.95; i += .1) {
      grad.addColorStop(i, "white");
      grad.addColorStop(i + .05, "black");
    }
    grad.addColorStop(1, "white");
    cx.fillStyle = grad;
    cx.fillRect(0, 0, s-1, s-1);
    cx.fillStyle = "rgba(200, 0, 0, 0.9)";
    cx.fillRect(.1 * s, .1 * s, .7 * s, .7 * s);
    cx.strokeStyle = "rgba(0, 0, 130, 0.5)";
    cx.lineWidth = .14 * s;
    cx.beginPath();
    cx.arc(.6 * s, .6 * s, .3 * s, 0, Math.PI*2, true);
    cx.stroke();
    cx.closePath();
    cx.fillStyle = "rgb(0, 255, 0)";
    cx.beginPath();
    cx.arc(.1 * s, .8 * s, .1 * s, 0, Math.PI*2, true);
    cx.fill();
    cx.closePath();

    let data = atob(cx.canvas.toDataURL("image/png").substring("data:text/png;base64,".length + 1));

    // This might fail if we dramatically improve the png encoder. If that happens
    // please increase the complexity or size of the image generated above to ensure
    // that we're testing with files that are large enough.
    ok(data.length > 65536, "test data sufficiently large");

    resolve(data);
  });
}

function createFile(data, name) {
  return new Promise(resolve => {
    SpecialPowers.createFiles([{name, data}],
                              files => { resolve(files[0]); });
  });
}
