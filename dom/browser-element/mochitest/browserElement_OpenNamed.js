/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 742944 - In <iframe mozbrowser>, test that if we call window.open twice
// with the same name, we get only one mozbrowseropenwindow event.

"use strict";
SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

var iframe;
var popupFrame;
function runTest() {
  iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');

  var gotPopup = false;
  iframe.addEventListener('mozbrowseropenwindow', function(e) {
    is(gotPopup, false, 'Should get just one popup.');
    gotPopup = true;
    popupFrame = e.detail.frameElement;
    is(popupFrame.getAttribute('name'), 'OpenNamed');

    // Called when file_browserElement_OpenNamed2.html loads into popupFrame.
    popupFrame.addEventListener('mozbrowsershowmodalprompt', function promptlistener(e) {
      popupFrame.removeEventListener('mozbrowsershowmodalprompt', promptlistener);

      ok(gotPopup, 'Got openwindow event before showmodalprompt event.');
      is(e.detail.message, 'success: loaded');
      SimpleTest.executeSoon(test2);
    });

    document.body.appendChild(popupFrame);
  });

  // OpenNamed.html will call
  //
  //    window.open('file_browserElement_OpenNamed2.html', 'OpenNamed').
  //
  // Once that popup loads, we reload OpenNamed.html.  That will call
  // window.open again, but we shouldn't get another openwindow event, because
  // we're opening into the same named window.
  iframe.src = 'file_browserElement_OpenNamed.html';
  document.body.appendChild(iframe);
}

function test2() {
  popupFrame.addEventListener('mozbrowsershowmodalprompt', function(e) {
    is(e.detail.message, 'success: loaded');
    SimpleTest.finish();
  });

  iframe.src = 'file_browserElement_OpenNamed.html?test2';
}

addEventListener('testready', runTest);
