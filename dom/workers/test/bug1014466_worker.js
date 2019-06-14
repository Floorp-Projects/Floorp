/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function ok(a, msg) {
  postMessage({type: "status", status: !!a, msg: msg });
}

onmessage = function(event) {

  function getResponse(url) {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", url, false);
    xhr.send();
    return xhr.responseText;
  }

  const testFile1 = "bug1014466_data1.txt";
  const testFile2 = "bug1014466_data2.txt";
  const testData1 = getResponse(testFile1);
  const testData2 = getResponse(testFile2);

  var response_count = 0;
  var xhr = new XMLHttpRequest();
  xhr.onreadystatechange = function() {
    if (xhr.readyState == xhr.DONE && xhr.status == 200) {
      response_count++;
      switch (response_count) {
        case 1:
          ok(xhr.responseText == testData1, "Check data 1");
          test_data2();
          break;
        case 2:
          ok(xhr.responseText == testData2, "Check data 2");
          postMessage({type: "finish" });
          break;
        default:
          ok(false, "Unexpected response received");
          postMessage({type: "finish" });
          break;
      }
    }
  }
  xhr.onerror = function(e) {
    ok(false, "Got an error event: " + e);
    postMessage({type: "finish" });
  }

  function test_data1() {
    xhr.open("GET", testFile1, true);
    xhr.responseType = "text";
    xhr.send();
  }

  function test_data2() {
    xhr.abort();
    xhr.open("GET", testFile2, true);
    xhr.responseType = "text";
    xhr.send();
  }

  test_data1();
}
