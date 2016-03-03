/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  let frameUrl = SimpleTest.getTestFileURL('/file_empty.html');
  SpecialPowers.pushPermissions([{
    type: 'browser:embedded-system-app',
    allow: true,
    context: {
      url: frameUrl,
      originAttributes: {
        inIsolatedMozBrowser: true
      }
    }
  },{
    type: 'browser',
    allow: true,
    context: {
      url: frameUrl,
      originAttributes: {
        inIsolatedMozBrowser: true
      }
    }
  }], createFrame);
}

var frame;
var mm;

function createFrame() {
  let loadEnd = function() {
    // The frame script running in the frame where the input is hosted.
    let appFrameScript = function appFrameScript() {
      let i = 1;

      addMessageListener('test:next', function() {
        try {
          switch (i) {
            case 1:
              content.document.addEventListener(
                'visibilitychange', function fn() {
                content.document.removeEventListener('visibilitychange', fn);
                sendAsyncMessage('test:done', {
                  ok: true,
                  msg: 'setVisible()'
                });
              });

              // Test a no return method
              content.navigator.mozBrowserElementProxy.setVisible(false);

              break;
            case 2:
              // Test a DOMRequest method
              var req = content.navigator.mozBrowserElementProxy.getVisible();
              req.onsuccess = function() {
                sendAsyncMessage('test:done', {
                  ok: true,
                  msg: 'getVisible()'
                });
              };

              req.onerror = function() {
                sendAsyncMessage('test:done', {
                  ok: false,
                  msg: 'getVisible() DOMRequest return error.'
                });
              };

              break;
            case 3:
              // Test an unimplemented method
              try {
                content.navigator.mozBrowserElementProxy.getActive();
                sendAsyncMessage('test:done', {
                  ok: false,
                  msg: 'getActive() should throw.'
                });

              } catch (e) {
                sendAsyncMessage('test:done', {
                  ok: true,
                  msg: 'getActive() throws.'
                });
              }

              break;
            case 4:
              content.navigator.mozBrowserElementProxy
              .addEventListener(
                'mozbrowserlocationchange',
                function() {
                  content.navigator.mozBrowserElementProxy
                    .onmozbrowserlocationchange = null;

                  sendAsyncMessage('test:done', {
                    ok: true,
                    msg: 'mozbrowserlocationchange'
                  });
              });

              // Test event
              content.location.hash = '#foo';

              break;
          }
        } catch (e) {
          sendAsyncMessage('test:done', {
            ok: false, msg: 'thrown: ' + e.toString() });
        }

        i++;
      });

      sendAsyncMessage('test:done', {});
    }

    mm = SpecialPowers.getBrowserFrameMessageManager(frame);
    mm.loadFrameScript('data:,(' + encodeURIComponent(appFrameScript.toString()) + ')();', false);
    mm.addMessageListener("test:done", next);
  };

  frame = document.createElement('iframe');
  frame.setAttribute('mozbrowser', 'true');
  frame.src = 'file_empty.html';
  document.body.appendChild(frame);
  frame.addEventListener('mozbrowserloadend', loadEnd);
}

var i = 0;
function next(msg) {
  let wrappedMsg = SpecialPowers.wrap(msg);
  let isOK = wrappedMsg.data.ok;
  let msgString = wrappedMsg.data.msg;

  switch (i) {
    case 0:
      mm.sendAsyncMessage('test:next');
      break;

    case 1:
    case 2:
    case 3:
      ok(isOK, msgString);
      mm.sendAsyncMessage('test:next');
      break;

    case 4:
      ok(isOK, msgString);
      SimpleTest.finish();
      break;
  }

  i++;
}

addEventListener('testready', runTest);
