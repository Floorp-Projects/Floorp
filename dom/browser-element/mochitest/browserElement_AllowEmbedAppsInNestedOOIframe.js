/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 1097479 - Allow embed remote apps or widgets in content
// process if nested-oop is enabled

"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  var iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');
  iframe.addEventListener('mozbrowsershowmodalprompt', function(e) {
    is(e.detail.message == 'app', true, e.detail.message);
    SimpleTest.finish();
  });

  document.body.appendChild(iframe);

  var context = {url: 'http://example.org',
                 originAttributes: {inIsolatedMozBrowser: true}};
  SpecialPowers.pushPermissions([
    {type: 'browser', allow: 1, context: context},
    {type: 'embed-apps', allow: 1, context: context}
  ], function() {
    iframe.src = 'http://example.org/tests/dom/browser-element/mochitest/file_browserElement_AllowEmbedAppsInNestedOOIframe.html';
  });
}

addEventListener('testready', () => {
  SpecialPowers.pushPrefEnv({set: [["dom.ipc.tabs.nested.enabled", true]]}, runTest);
});
