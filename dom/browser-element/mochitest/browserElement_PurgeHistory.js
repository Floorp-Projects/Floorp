/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 807056 - [Browser] Clear History doesn't clear back/forward history in open tabs
// <iframe mozbrowser>.

"use strict";
SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);

var iframe;
function addOneShotIframeEventListener(event, fn) {
  function wrapper(e) {
    iframe.removeEventListener(event, wrapper);
    fn(e);
  };

  iframe.addEventListener(event, wrapper);
}

function runTest() {
  iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');
  // FIXME: Bug 1270790
  iframe.setAttribute('remote', 'true');

  addOneShotIframeEventListener('mozbrowserloadend', function() {
    SimpleTest.executeSoon(test2);
  });

  iframe.src = browserElementTestHelpers.emptyPage1;
  document.body.appendChild(iframe);
}

function purgeHistory(nextTest) {
  var seenCanGoBackResult = false;
  var seenCanGoForwardResult = false;

  iframe.purgeHistory().onsuccess = function(e) {
    ok(true, "The history has been purged");

    iframe.getCanGoBack().onsuccess = function(e) {
      is(e.target.result, false, "Iframe cannot go back");
      seenCanGoBackResult = true;
      maybeRunNextTest();
    };

    iframe.getCanGoForward().onsuccess = function(e) {
      is(e.target.result, false, "Iframe cannot go forward");
      seenCanGoForwardResult = true;
      maybeRunNextTest();
    };
  };

  function maybeRunNextTest() {
    if (seenCanGoBackResult && seenCanGoForwardResult) {
      nextTest();
    }
  }
}

function test2() {
  purgeHistory(test3);
}

function test3() {
  addOneShotIframeEventListener('mozbrowserloadend', function() {
    purgeHistory(test4);
  });

  SimpleTest.executeSoon(function() {
    iframe.src = browserElementTestHelpers.emptyPage2;
  });
}

function test4() {
  addOneShotIframeEventListener('mozbrowserlocationchange', function(e) {
    is(e.detail.url, browserElementTestHelpers.emptyPage3);
    purgeHistory(SimpleTest.finish);
  });

  SimpleTest.executeSoon(function() {
    iframe.src = browserElementTestHelpers.emptyPage3;
  });
}

addEventListener('testready', runTest);
