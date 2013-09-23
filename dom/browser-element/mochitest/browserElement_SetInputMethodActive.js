/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 905573 - Add setInputMethodActive to browser elements to allow gaia
// system set the active IME app.
'use strict';

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function setup() {
  SpecialPowers.setBoolPref("dom.mozInputMethod.enabled", true);
  SpecialPowers.setBoolPref("dom.mozInputMethod.testing", true);
  SpecialPowers.addPermission('inputmethod-manage', true, document);
}

function tearDown() {
  SpecialPowers.setBoolPref("dom.mozInputMethod.enabled", false);
  SpecialPowers.setBoolPref("dom.mozInputMethod.testing", false);
  SpecialPowers.removePermission("inputmethod-manage", document);
  SimpleTest.finish();
}

function runTest() {
  // Create an input field to receive string from input method iframes.
  let input = document.createElement('input');
  input.type = 'text';
  document.body.appendChild(input);

  // Create two input method iframes.
  let frames = [];
  for (let i = 0; i < 2; i++) {
    frames[i] = document.createElement('iframe');
    SpecialPowers.wrap(frames[i]).mozbrowser = true;
    // When the input method iframe is activated, it will send the URL
    // hash to current focused element. We set different hash to each
    // iframe so that iframes can be differentiated by their hash.
    frames[i].src = 'file_inputmethod.html#' + i;
    frames[i].setAttribute('mozapp', location.href.replace(/[^/]+$/, 'file_inputmethod.webapp'));
    document.body.appendChild(frames[i]);
  }

  let count = 0;

  // Set focus to the input field and wait for input methods' inputting.
  SpecialPowers.DOMWindowUtils.focus(input);
  var timerId = null;
  input.oninput = function() {
    // The texts sent from the first and the second input method are '#0' and
    // '#1' respectively.
    switch (count) {
      case 1:
        is(input.value, '#0', 'Failed to get correct input from the first iframe.');
        testNextInputMethod();
        break;
      case 2:
        is(input.value, '#0#1', 'Failed to get correct input from the second iframe.');
        // Do nothing and wait for the next input from the second iframe.
        count++;
        break;
      case 3:
        is(input.value, '#0#1#1', 'Failed to get correct input from the second iframe.');
        // Receive the second input from the second iframe.
        count++;
        // Deactive the second iframe.
        frames[1].setInputMethodActive(false);
        // Wait for a short while to ensure the second iframe is not active any
        // more.
        timerId = setTimeout(function() {
          ok(true, 'Successfully deactivate the second iframe.');
          tearDown();
        }, 1000);
        break;
      default:
        ok(false, 'Failed to deactivate the second iframe.');
        clearTimeout(timerId);
        tearDown();
        break;
    }
  }

  ok(frames[0].setInputMethodActive, 'Cannot access setInputMethodActive.');

  function testNextInputMethod() {
    frames[count++].setInputMethodActive(true);
  }

  // Wait for a short while to let input method get ready.
  setTimeout(function() {
    testNextInputMethod();
  }, 500);
}

setup();
addEventListener('testready', runTest);
