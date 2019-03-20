/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that alertCheck (i.e., alert with the opportunity to opt out of future
// alerts), promptCheck, and confirmCheck work.  We do this by spamming
// alerts/prompts/confirms from inside an <iframe mozbrowser>.
//
// At the moment, we treat alertCheck/promptCheck/confirmCheck just like a
// normal alert.  But it's different to nsIPrompt!

"use strict";

/* global browserElementTestHelpers */

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();
browserElementTestHelpers.allowTopLevelDataURINavigation();

function runTest() {
  var iframe = document.createElement("iframe");
  iframe.setAttribute("mozbrowser", "true");
  document.body.appendChild(iframe);

  var numPrompts = 0;
  iframe.addEventListener("mozbrowsershowmodalprompt", function(e) {
    is(e.detail.message, String(numPrompts), "prompt message");
    if (numPrompts / 10 < 1) {
      is(e.detail.promptType, "alert");
    } else if (numPrompts / 10 < 2) {
      is(e.detail.promptType, "confirm");
    } else {
      is(e.detail.promptType, "prompt");
    }

    numPrompts++;
    if (numPrompts == 30) {
      SimpleTest.finish();
    }
  });

  /* eslint-disable no-useless-concat */
  iframe.src =
    'data:text/html,<html><body><script>\
      addEventListener("load", function() { \
       setTimeout(function() { \
        var i = 0; \
        for (; i < 10; i++) { alert(i); } \
        for (; i < 20; i++) { confirm(i); } \
        for (; i < 30; i++) { prompt(i); } \
       }); \
     }); \
     </scr' + "ipt></body></html>";
   /* eslint-enable no-useless-concat */
}

// The test harness sets dom.successive_dialog_time_limit to 0 for some bizarre
// reason.  That's not normal usage, and it keeps us from testing alertCheck!
addEventListener("testready", function() {
  SpecialPowers.pushPrefEnv({"set": [["dom.successive_dialog_time_limit", 10]]}, runTest);
});
