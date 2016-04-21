/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/*globals async, ok, is, SimpleTest, browserElementTestHelpers*/

// Bug 1169633 - getWebManifest tests
'use strict';
SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);

// request to load a manifest from a page that doesn't have a manifest.
// The expected result to be null.
var test1 = async(function* () {
  var manifest = yield requestManifest('file_empty.html');
  is(manifest, null, 'it should be null.');
});

// request to load a manifest from a page that has a manifest.
// The expected manifest to have a property name whose value is 'pass'.
var test2 = async(function* () {
  var manifest = yield requestManifest('file_web_manifest.html');
  is(manifest && manifest.name, 'pass', 'it should return a manifest with name pass.');
});

// Cause an exception by attempting to fetch a file URL,
// expect onerror to be called.
var test3 = async(function* () {
  var gotError = false;
  try {
    yield requestManifest('file_illegal_web_manifest.html');
  } catch (err) {
    gotError = true;
  }
  ok(gotError, 'onerror was called on the DOMRequest.');
});

// Run the tests
addEventListener('testready', () => {
  Promise
    .all([test1(), test2(), test3()])
    .then(SimpleTest.finish);
});

function requestManifest(url) {
  var iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');
  iframe.src = url;
  document.body.appendChild(iframe);
  return new Promise((resolve, reject) => {
    iframe.addEventListener('mozbrowserloadend', function loadend() {
      iframe.removeEventListener('mozbrowserloadend', loadend);
      SimpleTest.executeSoon(() => {
        var req = iframe.getWebManifest();
        req.onsuccess = () => {
          document.body.removeChild(iframe);
          resolve(req.result);
        };
        req.onerror = () => {
          document.body.removeChild(iframe);
          reject(new Error(req.error));
        };
      });
    });
  });
}
