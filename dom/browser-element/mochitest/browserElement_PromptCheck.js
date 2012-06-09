/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that alertCheck (i.e., alert with the opportunity to opt out of future
// alerts), promptCheck, and confirmCheck work.  We do this by spamming
// alerts/prompts/confirms from inside an <iframe mozbrowser>.
//
// At the moment, we treat alertCheck/promptCheck/confirmCheck just like a
// normal alert.  But it's different to nsIPrompt!
"use strict";

SimpleTest.waitForExplicitFinish();

browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addToWhitelist();

// Test harness sets dom.successive_dialog_time_limit to 0 for some bizarre
// reason.  That's not normal usage, and it keeps us from testing alertCheck!
const dialogTimeLimitPrefName = 'dom.successive_dialog_time_limit';
var oldDialogTimeLimitPref;
try {
  oldDialogTimeLimitPref = SpecialPowers.getIntPref(dialogTimeLimitPrefName);
}
catch(e) {}

SpecialPowers.setIntPref(dialogTimeLimitPrefName, 10);

var iframe = document.createElement('iframe');
iframe.mozbrowser = true;
document.body.appendChild(iframe);

var numPrompts = 0;
iframe.addEventListener('mozbrowsershowmodalprompt', function(e) {
  is(e.detail.message, numPrompts, "prompt message");
  if (numPrompts / 10 < 1) {
    is(e.detail.promptType, 'alert');
  }
  else if (numPrompts / 10 < 2) {
    is(e.detail.promptType, 'confirm');
  }
  else {
    is(e.detail.promptType, 'prompt');
  }

  numPrompts++;
  if (numPrompts == 30) {
    if (oldDialogTimeLimitPref !== undefined) {
      SpecialPowers.setIntPref(dialogTimeLimitPrefName, oldDialogTimeLimitPref);
    }
    else {
      SpecialPowers.clearUserPref(dialogTimeLimitPrefName);
    }

    SimpleTest.finish();
  }
});

iframe.src =
  'data:text/html,<html><body><script>\
    var i = 0; \
    for (; i < 10; i++) { alert(i); } \
    for (; i < 20; i++) { confirm(i); } \
    for (; i < 30; i++) { prompt(i); } \
   </scr' + 'ipt></body></html>';
