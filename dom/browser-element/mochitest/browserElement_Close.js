// Test that window.close() works.
"use strict";

SimpleTest.waitForExplicitFinish();

function runTest() {
  browserFrameHelpers.setEnabledPref(true);
  browserFrameHelpers.addToWhitelist();

  var iframe = document.createElement('iframe');
  iframe.mozbrowser = true;
  document.body.appendChild(iframe);

  iframe.addEventListener("mozbrowserclose", function(e) {
    ok(true, "got mozbrowserclose event.");
    SimpleTest.finish();
  });

  iframe.src = "data:text/html,<html><body><script>window.close()</scr"+"ipt></body></html>";
}

runTest();
