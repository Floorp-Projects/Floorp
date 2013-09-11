/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 780351 - Test that mozapp divides the window name namespace.
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();
// Permission to embed an app.
SpecialPowers.addPermission("embed-apps", true, document);

function runTest() {

  var iframe1 = document.createElement('iframe');
  SpecialPowers.wrap(iframe1).mozbrowser = true;
  iframe1.setAttribute('mozapp', 'http://example.org/manifest.webapp');

  // Two mozapp frames for different apps with the same code both do the same
  // window.open("foo", "bar") call.  We should get two mozbrowseropenwindow
  // events.

  iframe1.addEventListener('mozbrowseropenwindow', function(e) {
    ok(true, "Got first mozbrowseropenwindow event.");
    document.body.appendChild(e.detail.frameElement);

    SimpleTest.executeSoon(function() {
      var iframe2 = document.createElement('iframe');
      SpecialPowers.wrap(iframe2).mozbrowser = true;
      iframe2.setAttribute('mozapp', 'http://example.com/manifest.webapp');

      iframe2.addEventListener('mozbrowseropenwindow', function(e) {
        ok(true, "Got second mozbrowseropenwindow event.");
        SpecialPowers.removePermission("embed-apps", document);

        // We're not going to open this, but we don't want the platform to either
        e.preventDefault();
        SimpleTest.finish();
      });

      document.body.appendChild(iframe2);
      iframe2.src = 'http://example.com/tests/dom/browser-element/mochitest/file_browserElement_AppWindowNamespace.html';
    });
  });

  document.body.appendChild(iframe1);
  iframe1.src = 'http://example.org/tests/dom/browser-element/mochitest/file_browserElement_AppWindowNamespace.html';
}

addEventListener('testready', runTest);
