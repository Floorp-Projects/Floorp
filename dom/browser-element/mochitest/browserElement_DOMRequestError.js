/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test if DOMRequest returned by an iframe gets an error callback when
// the iframe is not in the DOM.
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);

function runTest() {
  var iframe1 = document.createElement('iframe');
  iframe1.setAttribute('mozbrowser', 'true');
  iframe1.src = 'data:text/html,<html>' +
    '<body style="background:green">hello</body></html>';
  document.body.appendChild(iframe1);

  function testIframe(beforeRun, isErrorExpected, nextTest) {
    return function() {
      var error = false;
      if (beforeRun)
        beforeRun();
      function testEnd() {
        is(isErrorExpected, error);
        SimpleTest.executeSoon(nextTest);
      }

      var domRequest = iframe1.getScreenshot(1000, 1000);
      domRequest.onsuccess = function(e) {
        testEnd();
      }
      domRequest.onerror = function(e) {
        error = true;
        testEnd();
      }
    };
  }

  function iframeLoadedHandler() {
    iframe1.removeEventListener('mozbrowserloadend', iframeLoadedHandler);
    // Test 1: iframe is in the DOM.
    // Test 2: iframe is removed from the DOM.
    // Test 3: iframe is added back into the DOM.
    var test3 = testIframe(
      function() {
        document.body.appendChild(iframe1);
      }, false,
      function() {
        SimpleTest.finish();
      })
    ;
    var test2 = testIframe(function() {
      document.body.removeChild(iframe1);
    }, true, test3);
    var test1 = testIframe(null, false, test2);
    SimpleTest.executeSoon(test1);
  }

  iframe1.addEventListener('mozbrowserloadend', iframeLoadedHandler);
}

addEventListener('testready', runTest);
