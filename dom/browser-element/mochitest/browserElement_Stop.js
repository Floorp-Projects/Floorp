/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 709759 - Test the stop ability of <iframe mozbrowser>.

// The img that is loaded will never be returned and will block
// the page from loading, the timeout ensures that the page is
// actually blocked from loading, once stop is called the
// image load will be cancaelled and mozbrowserloadend should be called.

"use strict";

/* global browserElementTestHelpers */

SimpleTest.waitForExplicitFinish();
SimpleTest.requestFlakyTimeout("untriaged");
browserElementTestHelpers.setEnabledPref(true);

var iframe;
var stopped = false;
var imgSrc = "http://test/tests/dom/browser-element/mochitest/file_bug709759.sjs";

function runTest() {
  iframe = document.createElement("iframe");
  iframe.setAttribute("mozbrowser", "true");
  // FIXME: Bug 1270790
  iframe.setAttribute("remote", "true");
  iframe.addEventListener("mozbrowserloadend", loadend);
  iframe.src = "data:text/html,<html>" +
    '<body><img src="' + imgSrc + '" /></body></html>';

  document.body.appendChild(iframe);

  setTimeout(function() {
    stopped = true;
    iframe.stop();
  }, 200);
}

function loadend() {
  ok(stopped, "Iframes network connections were stopped");

  // Wait 1 second and make sure there isn't a mozbrowsererror after stop();
  iframe.addEventListener("mozbrowsererror", handleError);
  window.setTimeout(function() {
    iframe.removeEventListener("mozbrowsererror", handleError);
    SimpleTest.finish();
  }, 1000);
}

function handleError() {
  ok(false, "mozbrowsererror should not be fired");
}

addEventListener("testready", runTest);
