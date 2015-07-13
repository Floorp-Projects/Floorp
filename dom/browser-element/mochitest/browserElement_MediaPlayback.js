/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the mozbrowsermediaplaybackchange event is fired correctly.
'use strict';

const { Services } = SpecialPowers.Cu.import('resource://gre/modules/Services.jsm');

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

/**
 * Content script passed to the child iframe
 */
function playMediaScript() {
  var audio = new content.Audio();
  content.document.body.appendChild(audio);
  audio.oncanplay = function() {
    audio.play();
  };
  audio.src = 'audio.ogg';
}

/**
 * Creates a simple mozbrowser frame
 */
function createFrame() {
  let iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');
  document.body.appendChild(iframe);
  return iframe;
}

function runTest() {
  SimpleTest.waitForExplicitFinish();

  let iframe = createFrame();
  let iframe2 = createFrame();

  // When the first iframe is finished loading inject a script to create
  // an audio element and play it.
  iframe.addEventListener('mozbrowserloadend', () => {
    let mm = SpecialPowers.getBrowserFrameMessageManager(iframe);
    mm.loadFrameScript('data:,(' + playMediaScript.toString() + ')();', false);
  });

  // Two events should come in, when the audio starts, and stops playing.
  // The first one should have a detail of 'active' and the second one
  // should have a detail of 'inactive'.
  let expectedNextData = 'active';
  iframe.addEventListener('mozbrowsermediaplaybackchange', (e) => {
    is(e.detail, expectedNextData, 'Audio detail should be correct')
    is(e.target, iframe, 'event target should be the first iframe')
    if (e.detail === 'inactive') {
      SimpleTest.finish();
    }
    expectedNextData = 'inactive';
  });

  // Make sure an event only goes to the first iframe.
  iframe2.addEventListener('mozbrowsermediaplaybackchange', (e) => {
    ok(false,
       'mozbrowsermediaplaybackchange should dispatch to the correct browser');
  });

  // Load a simple page to get the process started.
  iframe.src = browserElementTestHelpers.emptyPage1;
}

addEventListener('testready', () => {
  // Audio channel service is needed for events
  SpecialPowers.pushPrefEnv({"set": [["media.useAudioChannelService", true]]},
                            runTest);
});
