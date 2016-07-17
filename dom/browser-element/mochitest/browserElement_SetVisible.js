/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the setVisible property for mozbrowser
"use strict";

SimpleTest.waitForExplicitFinish();
SimpleTest.requestFlakyTimeout("untriaged");
browserElementTestHelpers.setEnabledPref(true);

var iframeScript = function() {
  content.document.addEventListener("visibilitychange", function() {
    sendAsyncMessage('test:visibilitychange', {
      hidden: content.document.hidden
    });
  }, false);
}

function runTest() {
  var mm;
  var numEvents = 0;
  var iframe1 = document.createElement('iframe');
  iframe1.setAttribute('mozbrowser', 'true');
  iframe1.src = 'data:text/html,1';

  document.body.appendChild(iframe1);

  function recvVisibilityChanged(msg) {
    msg = SpecialPowers.wrap(msg);
    numEvents++;
    if (numEvents === 1) {
      ok(true, 'iframe recieved visibility changed');
      ok(msg.json.hidden === true, 'hidden attribute correctly set');
      iframe1.setVisible(false);
      iframe1.setVisible(true);
    } else if (numEvents === 2) {
      ok(msg.json.hidden === false, 'hidden attribute correctly set');
      // Allow some time in case we generate too many events
      setTimeout(function() {
        mm.removeMessageListener('test:visibilitychange', recvVisibilityChanged);
        SimpleTest.finish();
      }, 100);
    } else {
      ok(false, 'Too many visibilitychange events');
    }
  }

  function iframeLoaded() {
    testGetVisible();
  }

  function testGetVisible() {
    iframe1.setVisible(false);
    iframe1.getVisible().onsuccess = function(evt) {
      ok(evt.target.result === false, 'getVisible() responds false after setVisible(false)');

      iframe1.setVisible(true);
      iframe1.getVisible().onsuccess = function(evt) {
        ok(evt.target.result === true, 'getVisible() responds true after setVisible(true)');
        testVisibilityChanges();
      };
    };
  }

  function testVisibilityChanges() {
    mm = SpecialPowers.getBrowserFrameMessageManager(iframe1);
    mm.addMessageListener('test:visibilitychange', recvVisibilityChanged);
    mm.loadFrameScript('data:,(' + iframeScript.toString() + ')();', false);
    iframe1.setVisible(false);
  }

  iframe1.addEventListener('mozbrowserloadend', iframeLoaded);
}

addEventListener('testready', runTest);
