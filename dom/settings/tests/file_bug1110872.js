"use strict";

SimpleTest.waitForExplicitFinish();

var iframe;
var loadedEvents = 0;

function loadServer() {
  var url = SimpleTest.getTestFileURL("file_loadserver.js");
  var script = SpecialPowers.loadChromeScript(url);
}

function runTest() {
  iframe = document.createElement('iframe');
  document.body.appendChild(iframe);
  iframe.addEventListener('load', mozbrowserLoaded);
  iframe.src = 'file_bug1110872.html';
}

function iframeBodyRecv(msg) {
  switch (loadedEvents) {
  case 1:
    // If we get a message back before we've seen 2 loads, that means
    // something went wrong with the test. Fail immediately.
    ok(true, 'got response from first test!');
    break;
  case 2:
    // If we get a message back after 2 loads (initial load, reload),
    // it means the callback for the last lock fired, which means the
    // SettingsRequestManager queue has to have been cleared
    // correctly.
    ok(true, 'further queries returned ok after SettingsManager death');
    SimpleTest.finish();
    break;
  }
}

function mozbrowserLoaded() {
  loadedEvents++;
  iframe.contentWindow.postMessage({name: "start", step: loadedEvents}, '*');
  window.addEventListener('message', iframeBodyRecv);
}

window.addEventListener("load", function() {
  loadServer();
  runTest();
});
