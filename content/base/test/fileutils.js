// Utility functions
var testRanCounter = 0;
var expectedTestCount = 0;

function testHasRun() {
 //alert(testRanCounter);
 ++testRanCounter;
 if (testRanCounter == expectedTestCount) {
    SimpleTest.finish();
  }
}


function testFile(file, contents, test) {
  SimpleTest.requestLongerTimeout(2);

  // Load file using FileReader
  var r = new FileReader();
  r.onload = getFileReaderLoadHandler(contents, contents.length, "FileReader.readAsBinaryString of " + test);
  r.readAsBinaryString(file);
  expectedTestCount++;

  // Load file using URL.createObjectURL and XMLHttpRequest
  var xhr = new XMLHttpRequest;
  xhr.open("GET", URL.createObjectURL(file));
  xhr.onload = getXHRLoadHandler(contents, contents.length, false,
                                 "XMLHttpRequest load of " + test);
  xhr.overrideMimeType('text/plain; charset=x-user-defined');
  xhr.send();
  expectedTestCount++;

  // Send file to server using FormData and XMLHttpRequest
  xhr = new XMLHttpRequest();
  xhr.onload = function(event) {
    checkMPSubmission(JSON.parse(event.target.responseText),
                      [{ name: "hello", value: "world"},
                       { name: "myfile",
                         value: contents,
                         fileName: file.name || "blob",
                         contentType: file.type || "application/octet-stream" }]);
    testHasRun();
  }
  xhr.open("POST", "../../html/content/test/form_submit_server.sjs");
  var fd = new FormData;
  fd.append("hello", "world");
  fd.append("myfile", file);
  xhr.send(fd);
  expectedTestCount++;

  // Send file to server using plain XMLHttpRequest
  var xhr = new XMLHttpRequest;
  xhr.open("POST", "file_XHRSendData.sjs");
  xhr.onload = function (event) {
    is(event.target.getResponseHeader("Result-Content-Type"),
       file.type ? file.type : null,
       "request content-type in XMLHttpRequest send of " + test);
    is(event.target.getResponseHeader("Result-Content-Length"),
       file.size,
       "request content-length in XMLHttpRequest send of " + test);
  };
  xhr.addEventListener("load",
                       getXHRLoadHandler(contents, contents.length, true,
                                         "XMLHttpRequest send of " + test),
                       false);
  xhr.overrideMimeType('text/plain; charset=x-user-defined');
  xhr.send(file);
  expectedTestCount++;
}

function getFileReaderLoadHandler(expectedResult, expectedLength, testName) {
  return function (event) {
    is(event.target.readyState, FileReader.DONE,
       "[FileReader] readyState in test " + testName);
    is(event.target.error, null,
       "[FileReader] no error in test " + testName);
    // Do not use |is(event.target.result, expectedResult, "...");| that may output raw binary data.
    is(event.target.result.length, expectedResult.length,
       "[FileReader] Length of result in test " + testName);
    ok(event.target.result == expectedResult,
       "[FileReader] Content of result in test " + testName);
    is(event.lengthComputable, true,
       "[FileReader] lengthComputable in test " + testName);
    is(event.loaded, expectedLength,
       "[FileReader] Loaded length in test " + testName);
    is(event.total, expectedLength,
       "[FileReader] Total length in test " + testName);
    testHasRun();
  }
}

function getXHRLoadHandler(expectedResult, expectedLength, statusWorking, testName) {
  return function (event) {
    is(event.target.readyState, 4,
       "[XHR] readyState in test " + testName);
    if (statusWorking) {
      is(event.target.status, 200,
         "[XHR] no error in test " + testName);
    }
    else {
      todo_is(event.target.status, 200,
              "[XHR] no error in test " + testName);
    }
    // Do not use |is(convertXHRBinary(event.target.responseText), expectedResult, "...");| that may output raw binary data.
    var convertedData = convertXHRBinary(event.target.responseText);
    is(convertedData.length, expectedResult.length,
       "[XHR] Length of result in test " + testName);
    ok(convertedData == expectedResult,
       "[XHR] Content of result in test " + testName);
    is(event.lengthComputable, true,
       "[XHR] lengthComputable in test " + testName);
    is(event.loaded, expectedLength,
       "[XHR] Loaded length in test " + testName);
    is(event.total, expectedLength,
       "[XHR] Total length in test " + testName);

    testHasRun();
  }
}

function convertXHRBinary(s) {
  var res = "";
  for (var i = 0; i < s.length; ++i) {
    res += String.fromCharCode(s.charCodeAt(i) & 255);
  }
  return res;
}

function testHasRun() {
 //alert(testRanCounter);
 ++testRanCounter;
 if (testRanCounter == expectedTestCount) {
    SimpleTest.finish();
  }
}

function createFileWithData(fileData) {
  var dirSvc = SpecialPowers.Cc["@mozilla.org/file/directory_service;1"].getService(SpecialPowers.Ci.nsIProperties);
  var testFile = dirSvc.get("ProfD", SpecialPowers.Ci.nsIFile);
  testFile.append("fileAPItestfile2-" + fileNum);
  fileNum++;
  var outStream = SpecialPowers.Cc["@mozilla.org/network/file-output-stream;1"].createInstance(SpecialPowers.Ci.nsIFileOutputStream);
  outStream.init(testFile, 0x02 | 0x08 | 0x20, // write, create, truncate
                 0666, 0);
  outStream.write(fileData, fileData.length);
  outStream.close();

  var fileList = document.getElementById('fileList');
  SpecialPowers.wrap(fileList).value = testFile.path;

  return fileList.files[0];
}

function gc() {
  window.QueryInterface(SpecialPowers.Ci.nsIInterfaceRequestor)
        .getInterface(SpecialPowers.Ci.nsIDOMWindowUtils)
        .garbageCollect();
}

function checkMPSubmission(sub, expected) {
  function getPropCount(o) {
    var x, l = 0;
    for (x in o) ++l;
    return l;
  }

  is(sub.length, expected.length,
     "Correct number of items");
  var i;
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

function testSlice(file, size, type, contents, fileType) {
  is(file.type, type, fileType + " file is correct type");
  is(file.size, size, fileType + " file is correct size");
  ok(file instanceof File, fileType + " file is a File");
  ok(file instanceof Blob, fileType + " file is also a Blob");
  
  var slice = file.slice(0, size);
  ok(slice instanceof Blob, fileType + " fullsize slice is a Blob");
  ok(!(slice instanceof File), fileType + " fullsize slice is not a File");

  // Test that mozSlice works still.
  slice = file.mozSlice(0, size);
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
  var indexes = [[0, size, size],
                 [0, 1234, 1234],
                 [size-500, size, 500],
                 [size-500, size+500, 500],
                 [size+500, size+1500, 0],
                 [0, 0, 0],
                 [1000, 1000, 0],
                 [size, size, 0],
                 [undefined, undefined, size],
                 [0, undefined, size],
                 [100, undefined, size-100],
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
  
  for (var i = 0; i < indexes.length; ++i) {
    var sliceContents;
    var testName;
    if (indexes[i][0] == undefined) {
      slice = file.slice();
      sliceContents = contents.slice();
      testName = fileType + " slice()";
    }
    else if (indexes[i][1] == undefined) {
      slice = file.slice(indexes[i][0]);
      sliceContents = contents.slice(indexes[i][0]);
      testName = fileType + " slice(" + indexes[i][0] + ")";
    }
    else {
      slice = file.slice(indexes[i][0], indexes[i][1]);
      sliceContents = contents.slice(indexes[i][0], indexes[i][1]);
      testName = fileType + " slice(" + indexes[i][0] + ", " + indexes[i][1] + ")";
    }
    is(slice.type, "", testName + " type");
    is(slice.size, indexes[i][2], testName + " size");
    is(sliceContents.length, indexes[i][2], testName + " data size");
    testFile(slice, sliceContents, testName);
  }

  // Slice of slice
  var slice = file.slice(0, 40000);
  testFile(slice.slice(5000, 42000), contents.slice(5000, 40000), "file slice slice");
  
  // ...of slice of slice
  slice = slice.slice(5000, 42000).slice(400, 700);
  SpecialPowers.gc();
  testFile(slice, contents.slice(5400, 5700), "file slice slice slice");
}

