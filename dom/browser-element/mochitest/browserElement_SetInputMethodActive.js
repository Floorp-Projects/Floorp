/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 905573 - Add setInputMethodActive to browser elements to allow gaia
// system set the active IME app.
'use strict';

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

// We'll need to get the appId from the current document,
// it's either SpecialPowers.Ci.nsIScriptSecurityManager.NO_APP_ID when
// we are not running inside an app (e.g. Firefox Desktop),
// or the appId of Mochitest app when we are running inside that app
// (e.g. Emulator).
var currentAppId = SpecialPowers.wrap(document).nodePrincipal.appId;
var inApp =
  currentAppId !== SpecialPowers.Ci.nsIScriptSecurityManager.NO_APP_ID;
// We will also need the manifest URL and set it on iframes.
var currentAppManifestURL;

if (inApp) {
  let appsService = SpecialPowers.Cc["@mozilla.org/AppsService;1"]
                      .getService(SpecialPowers.Ci.nsIAppsService);

  currentAppManifestURL = appsService.getManifestURLByLocalId(currentAppId);
};

info('appId=' + currentAppId);
info('manifestURL=' + currentAppManifestURL);

function setup() {
  let appInfo = SpecialPowers.Cc['@mozilla.org/xre/app-info;1']
                .getService(SpecialPowers.Ci.nsIXULAppInfo);
  if (appInfo.name != 'B2G') {
    SpecialPowers.Cu.import("resource://gre/modules/Keyboard.jsm", window);
  }

  SpecialPowers.setBoolPref("dom.mozInputMethod.enabled", true);
  SpecialPowers.addPermission('input-manage', true, document);
}

function tearDown() {
  SpecialPowers.setBoolPref("dom.mozInputMethod.enabled", false);
  SpecialPowers.removePermission('input-manage', document);
  SimpleTest.finish();
}

function runTest() {
  createFrames();
}

var gInputMethodFrames = [];
var gInputFrame;

function createFrames() {
  // Create two input method iframes.
  let loadendCount = 0;
  let countLoadend = function() {
    loadendCount++;
    if (loadendCount === 3) {
      setPermissions();
     }
   };

   // Create an input field to receive string from input method iframes.
   gInputFrame = document.createElement('iframe');
   gInputFrame.setAttribute('mozbrowser', 'true');
   gInputFrame.src =
     'data:text/html,<input autofocus value="hello" />' +
     '<p>This is targetted mozbrowser frame.</p>';
   document.body.appendChild(gInputFrame);
   gInputFrame.addEventListener('mozbrowserloadend', countLoadend);

   for (let i = 0; i < 2; i++) {
    let frame = gInputMethodFrames[i] = document.createElement('iframe');
    frame.setAttribute('mozbrowser', 'true');
    if (currentAppManifestURL) {
      frame.setAttribute('mozapp', currentAppManifestURL);
    }
    frame.src = 'file_empty.html#' + i;
     document.body.appendChild(frame);
     frame.addEventListener('mozbrowserloadend', countLoadend);
   }
 }

function setPermissions() {
  let permissions = [{
    type: 'input',
    allow: true,
    context: {
      url: SimpleTest.getTestFileURL('/file_empty.html'),
      originAttributes: {
        appId: currentAppId,
        inBrowser: true
      }
    }
  }];

  if (inApp) {
    // The current document would also need to be given access for IPC to
    // recognize our permission (why)?
    permissions.push({
      type: 'input', allow: true, context: document });
  }

  SpecialPowers.pushPermissions(permissions,
    SimpleTest.waitForFocus.bind(SimpleTest, startTest));
}

 function startTest() {
  // The frame script running in the frame where the input is hosted.
  let appFrameScript = function appFrameScript() {
    let input = content.document.body.firstElementChild;
    input.oninput = function() {
      sendAsyncMessage('test:InputMethod:oninput', {
        from: 'input',
        value: this.value
      });
    };

    input.onblur = function() {
      // "Expected" lost of focus since the test is finished.
      if (input.value === '#0#1hello') {
        return;
      }

      sendAsyncMessage('test:InputMethod:oninput', {
        from: 'input',
        error: true,
        value: 'Unexpected lost of focus on the input frame!'
      });
    };

    input.focus();
  };

  // Inject frame script to receive input.
  let mm = SpecialPowers.getBrowserFrameMessageManager(gInputFrame);
  mm.loadFrameScript('data:,(' + encodeURIComponent(appFrameScript.toString()) + ')();', false);
  mm.addMessageListener("test:InputMethod:oninput", next);

  gInputMethodFrames.forEach((frame) => {
    ok(frame.setInputMethodActive, 'Can access setInputMethodActive.');

    // The frame script running in the input method frames.
    let appFrameScript = function appFrameScript() {
      let im = content.navigator.mozInputMethod;
      im.oninputcontextchange = function() {
        let ctx = im.inputcontext;
        // Report back to parent frame on status of ctx gotten.
        // (A setTimeout() here to ensure this always happens after
        //  DOMRequest succeed.)
        content.setTimeout(() => {
          sendAsyncMessage('test:InputMethod:imFrameMessage', {
            from: 'im',
            value: content.location.hash + !!ctx
          });
        });

        // If there is a context, send out the hash.
        if (ctx) {
          ctx.replaceSurroundingText(content.location.hash);
        }
      };
    };

    // Inject frame script to receive message.
    let mm = SpecialPowers.getBrowserFrameMessageManager(frame);
    mm.loadFrameScript('data:,(' + encodeURIComponent(appFrameScript.toString()) + ')();', false);
    mm.addMessageListener("test:InputMethod:imFrameMessage", next);
  });

   // Set focus to the input field and wait for input methods' inputting.
   SpecialPowers.DOMWindowUtils.focus(gInputFrame);

  let req0 = gInputMethodFrames[0].setInputMethodActive(true);
  req0.onsuccess = function() {
    ok(true, 'setInputMethodActive succeeded (0).');
  };

  req0.onerror = function() {
    ok(false, 'setInputMethodActive failed (0): ' + this.error.name);
  };
}

var gCount = 0;

var gFrameMsgCounts = {
  'input': 0,
  'im0': 0,
  'im1': 0
};

function next(msg) {
  let wrappedMsg = SpecialPowers.wrap(msg);
  let from = wrappedMsg.data.from;
  let value = wrappedMsg.data.value;

  if (wrappedMsg.data.error) {
    ok(false, wrappedMsg.data.value);

    return;
  }

  let fromId = from;
  if (from === 'im') {
    fromId += value[1];
  }
  gFrameMsgCounts[fromId]++;

  // The texts sent from the first and the second input method are '#0' and
  // '#1' respectively.
  switch (gCount) {
    case 0:
      switch (fromId) {
        case 'im0':
          if (gFrameMsgCounts.im0 === 1) {
            is(value, '#0true', 'First frame should get the context first.');
          } else {
            ok(false, 'Unexpected multiple messages from im0.')
          }

          break;

        case 'im1':
          is(false, 'Shouldn\'t be hearing anything from second frame.');

          break;

        case 'input':
          if (gFrameMsgCounts.input === 1) {
            is(value, '#0hello',
              'Failed to get correct input from the first iframe.');
          } else {
            ok(false, 'Unexpected multiple messages from input.')
          }

          break;
      }

      if (gFrameMsgCounts.input !== 1 ||
          gFrameMsgCounts.im0 !== 1 ||
          gFrameMsgCounts.im1 !== 0) {
        return;
      }

      gCount++;

      let req0 = gInputMethodFrames[0].setInputMethodActive(false);
      req0.onsuccess = function() {
        ok(true, 'setInputMethodActive succeeded (0).');
      };
      req0.onerror = function() {
        ok(false, 'setInputMethodActive failed (0): ' + this.error.name);
      };
      let req1 = gInputMethodFrames[1].setInputMethodActive(true);
      req1.onsuccess = function() {
        ok(true, 'setInputMethodActive succeeded (1).');
      };
      req1.onerror = function() {
        ok(false, 'setInputMethodActive failed (1): ' + this.error.name);
      };

      break;

    case 1:
      switch (fromId) {
        case 'im0':
          if (gFrameMsgCounts.im0 === 2) {
            is(value, '#0false', 'First frame should have the context removed.');
          } else {
            ok(false, 'Unexpected multiple messages from im0.')
          }
          break;

        case 'im1':
          if (gFrameMsgCounts.im1 === 1) {
            is(value, '#1true', 'Second frame should get the context.');
          } else {
            ok(false, 'Unexpected multiple messages from im0.')
          }

          break;

        case 'input':
          if (gFrameMsgCounts.input === 2) {
            is(value, '#0#1hello',
               'Failed to get correct input from the second iframe.');
          } else {
            ok(false, 'Unexpected multiple messages from input.')
          }
          break;
      }

      if (gFrameMsgCounts.input !== 2 ||
          gFrameMsgCounts.im0 !== 2 ||
          gFrameMsgCounts.im1 !== 1) {
        return;
      }

      gCount++;

      // Receive the second input from the second iframe.
      // Deactive the second iframe.
      let req3 = gInputMethodFrames[1].setInputMethodActive(false);
      req3.onsuccess = function() {
        ok(true, 'setInputMethodActive(false) succeeded (2).');
      };
      req3.onerror = function() {
        ok(false, 'setInputMethodActive(false) failed (2): ' + this.error.name);
      };
      break;

    case 2:
      is(fromId, 'im1', 'Message sequence unexpected (3).');
      is(value, '#1false', 'Second frame should have the context removed.');

      tearDown();
      break;
  }
}

setup();
addEventListener('testready', runTest);
