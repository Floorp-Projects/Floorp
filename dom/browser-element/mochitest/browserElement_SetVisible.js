/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the setVisible property for mozbrowser
"use strict";

SimpleTest.waitForExplicitFinish();

var iframeScript = function() {
  content.document.addEventListener("mozvisibilitychange", function() {
    sendAsyncMessage('test:visibilitychange', {
      hidden: content.document.mozHidden
    });
  }, false);
}

function runTest() {

  browserElementTestHelpers.setEnabledPref(true);
  browserElementTestHelpers.addToWhitelist();

  var mm;
  var numEvents = 0;
  var iframe1 = document.createElement('iframe');
  iframe1.mozbrowser = true;
  iframe1.src = 'data:text/html,1';

  document.body.appendChild(iframe1);

  function recvVisibilityChanged(msg) {
    numEvents++;
    if (numEvents === 1) {
      ok(true, 'iframe recieved visibility changed');
      ok(msg.json.hidden === true, 'mozHidden attribute correctly set');
      iframe1.setVisible(false);
      iframe1.setVisible(true);
    } else if (numEvents === 2) {
      ok(msg.json.hidden === false, 'mozHidden attribute correctly set');
      // Allow some time in case we generate too many events
      setTimeout(function() {
        mm.removeMessageListener('test:visibilitychange', recvVisibilityChanged);
        SimpleTest.finish();
      }, 100);
    } else {
      ok(false, 'Too many mozhidden events');
    }
  }

  function iframeLoaded() {
    mm = SpecialPowers.getBrowserFrameMessageManager(iframe1);
    mm.addMessageListener('test:visibilitychange', recvVisibilityChanged);
    mm.loadFrameScript('data:,(' + iframeScript.toString() + ')();', false);
    iframe1.setVisible(false);
  }

  iframe1.addEventListener('mozbrowserloadend', iframeLoaded);
}

addEventListener('load', function() { SimpleTest.executeSoon(runTest); });


