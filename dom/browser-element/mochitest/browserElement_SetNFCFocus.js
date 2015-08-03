/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 1188639 - Check permission to use setNFCFocus
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function hasSetNFCFocus() {
  return new Promise((resolve, reject) => {
    var iframe = document.createElement('iframe');
    iframe.setAttribute('mozbrowser', 'true');
    iframe.addEventListener('mozbrowserloadend', e => {
      is(iframe.setNFCFocus !== undefined, true,
         "has permission to use setNFCFocus");
      resolve();
    });
    document.body.appendChild(iframe);
  });
}

function runTest() {
  SpecialPowers.pushPermissions(
    [{ 'type': 'nfc-manager', 'allow': 1, 'context': document }],
    () => hasSetNFCFocus().then(SimpleTest.finish));
}

addEventListener('testready', runTest);
