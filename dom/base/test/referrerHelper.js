/**
 * Listen for notifications from the child.
 * These are sent in case of error, or when the loads we await have completed.
 */
window.addEventListener("message", function(event) {
    if (event.data == "childLoadComplete") {
      // all loads happen, continue the test.
      advance();
    } else if (event.data == "childOverload") {
      // too many loads happened in a test frame, abort.
      ok(false, "Too many load handlers called in test.");
      SimpleTest.finish();
    } else if (event.data.indexOf("fail-") == 0) {
      // something else failed in the test frame, abort.
      ok(false, "Child failed the test with error " + event.data.substr(5));
      SimpleTest.finish();
    }});


/**
 * helper to perform an XHR.
 * Used by resetCounter() and checkResults().
 */
function doXHR(url, onSuccess, onFail) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    if (xhr.status == 200) {
      onSuccess(xhr);
    } else {
      onFail(xhr);
    }
  };
  xhr.open('GET', url, true);
  xhr.send(null);
}



/**
 * This triggers state-resetting on the counter server.
 */
function resetCounter() {
  doXHR('/tests/dom/base/test/bug704320_counter.sjs?reset',
        advance,
        function(xhr) {
          ok(false, "Need to be able to reset the request counter");
          SimpleTest.finish();
        });
}

/**
 * Grabs the results via XHR and passes to checker.
 */
function checkResults(testname, expected) {
  doXHR('/tests/dom/base/test/bug704320_counter.sjs?results',
        function(xhr) {
          var results = JSON.parse(xhr.responseText);
          info(xhr.responseText);

          ok('img' in results,
              testname + " test: some image loads required in results object.");
          is(results['img'].count, 2,
            testname + " Test: Expected 2 loads for image requests.");

          expected.forEach(function (ref) {
            ok(results['img'].referrers.indexOf(ref) >= 0,
                testname + " Test: Expected " + ref + " referrer policy in test, results were " + 
                JSON.stringify(results['img'].referrers) +".");
            });
          advance();
        },
        function(xhr) {
          ok(false, "Can't get results from the counter server.");
          SimpleTest.finish();
        });
}
