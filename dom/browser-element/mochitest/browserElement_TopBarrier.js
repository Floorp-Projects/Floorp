/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that an <iframe mozbrowser> is a window.{top,parent,frameElement} barrier.
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

var iframe;
function runTest() {
  iframe = document.createElement('iframe');
  iframe.addEventListener('mozbrowserloadend', function() {
    try {
      outerIframeLoaded();
    } catch(e) {
      dump("Got error: " + e + '\n');
    }
  });
  SpecialPowers.wrap(iframe).mozbrowser = true;
  iframe.src = 'data:text/html,Outer iframe <iframe id="inner-iframe"></iframe>';
  // For kicks, this test uses a display:none iframe.  This shouldn't make a
  // difference in anything.
  iframe.style.display = 'none';
  document.body.appendChild(iframe);
}

var numMsgReceived = 0;
function outerIframeLoaded() {
  var injectedScript =
    "data:,function is(a, b, desc) {                                     \
      if (a == b) {                                                      \
        sendAsyncMessage('test:test-pass', desc);                        \
      } else {                                                           \
        sendAsyncMessage('test:test-fail', desc + ' ' + a + ' != ' + b); \
      }                                                                  \
    }                                                                    \
    is(content.window.top, content.window, 'top');                       \
    is(content.window.content, content.window, 'content');               \
    is(content.window.parent, content.window, 'parent');                 \
    is(content.window.frameElement, null, 'frameElement');               \
    var innerIframe = content.document.getElementById('inner-iframe');   \
    var innerWindow = innerIframe.contentWindow;                         \
    is(innerWindow.top, content.window, 'inner top');                    \
    is(innerWindow.content, content.window, 'inner content');            \
    is(innerWindow.parent, content.window, 'inner parent');              \
    is(innerWindow.frameElement, innerIframe, 'inner frameElement');"

  var mm = SpecialPowers.getBrowserFrameMessageManager(iframe);

  function onRecvTestPass(msg) {
    numMsgReceived++;
    ok(true, msg.json);
  }
  mm.addMessageListener('test:test-pass', onRecvTestPass);

  function onRecvTestFail(msg) {
    numMsgReceived++;
    ok(false, msg.json);
  }
  mm.addMessageListener('test:test-fail', onRecvTestFail);

  mm.loadFrameScript(injectedScript, /* allowDelayedLoad = */ false);

  waitForMessages(6);
}

function waitForMessages(num) {
  if (numMsgReceived < num) {
    SimpleTest.executeSoon(function() { waitForMessages(num); });
    return;
  }

  SimpleTest.finish();
}

addEventListener('testready', runTest);
