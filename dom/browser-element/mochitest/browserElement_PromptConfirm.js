/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that prompt and confirm work.  In particular, we're concerned that we
// get correct return values out of them.
//
// We use alert() to communicate the return values of prompt/confirm back to
// ourselves.
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

  var prompts = [
    {msg: "1", type: "alert", rv: 42, expected: "undefined"},
    {msg: "2", type: "confirm", rv: true, expected: "true"},
    {msg: "3", type: "confirm", rv: false, expected: "false"},

    // rv == 42 should be coerced to 'true' for confirm.
    {msg: "4", type: "confirm", rv: 42, expected: "true"},
    {msg: "5", type: "prompt", rv: "worked", expected: "worked"},
    {msg: "6", type: "prompt", rv: null, expected: "null"},
    {msg: "7", type: "prompt", rv: "", expected: ""},
  ];

  iframe.addEventListener("mozbrowsershowmodalprompt", function(e) {
    var curPrompt = prompts[0];
    if (!curPrompt.waitingForResponse) {
      curPrompt.waitingForResponse = true;

      is(e.detail.message, curPrompt.msg, "prompt message");
      is(e.detail.promptType, curPrompt.type, "prompt type");

      if (e.detail.promptType == "prompt") {
        ok(e.detail.returnValue === null, "prompt's returnValue should be null");
        is(e.detail.initialValue, "initial", "prompt's initial value.");
      } else {
        ok(e.detail.returnValue === undefined,
           "Other than for prompt, shouldn't have initial value.");
      }

      // Block the child until we call e.detail.unblock().
      e.preventDefault();

      SimpleTest.executeSoon(function() {
        e.detail.returnValue = curPrompt.rv;
        e.detail.unblock();
      });
    } else {
      prompts.shift();

      // |e| now corresponds to an alert() containing the return value we just
      // sent for this prompt.

      is(e.detail.message, "RESULT:" + curPrompt.expected,
         "expected rv for msg " + curPrompt.msg);

      if (prompts.length == 0) {
        SimpleTest.finish();
      }
    }
  });

  /* eslint-disable no-useless-concat */
  iframe.src =
    'data:text/html,<html><body><script>\
      function sendVal(val) { \
        alert("RESULT:" + val); \
      } \
      sendVal(alert("1")); \
      sendVal(confirm("2")); \
      sendVal(confirm("3")); \
      sendVal(confirm("4")); \
      sendVal(prompt("5", "initial")); \
      sendVal(prompt("6", "initial")); \
      sendVal(prompt("7", "initial")); \
    </scr' + "ipt></body></html>";
  /* eslint-enable no-useless-concat */
}

addEventListener("testready", runTest);
