/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that auth prompt works.
"use strict";

SimpleTest.waitForExplicitFinish();

function testFail(msg) {
  ok(false, JSON.stringify(msg));
}

var iframe;

function runTest() {
  browserElementTestHelpers.setEnabledPref(true);
  browserElementTestHelpers.addPermission();

  iframe = document.createElement('iframe');
  iframe.mozbrowser = true;
  document.body.appendChild(iframe);

  // Wait for the initial load to finish, then navigate the page, then start test
  // by loading SJS with http 401 response.
  iframe.addEventListener('mozbrowserloadend', function loadend() {
    iframe.removeEventListener('mozbrowserloadend', loadend);
    iframe.addEventListener('mozbrowserusernameandpasswordrequired', testHttpAuthCancel);
    SimpleTest.executeSoon(function() {
      // Use absolute path because we need to specify host.
      iframe.src = 'http://test/tests/dom/browser-element/mochitest/file_http_401_response.sjs';
    });
  });
}

function testHttpAuthCancel(e) {
  iframe.removeEventListener("mozbrowserusernameandpasswordrequired", testHttpAuthCancel);
  // Will cancel authentication, but prompt should not be shown again. Instead,
  // we will be led to fail message
  iframe.addEventListener("mozbrowserusernameandpasswordrequired", testFail);
  iframe.addEventListener("mozbrowsertitlechange", function onTitleChange(e) {
    iframe.removeEventListener("mozbrowsertitlechange", onTitleChange);
    iframe.removeEventListener("mozbrowserusernameandpasswordrequired", testFail);
    is(e.detail, 'http auth failed');
    iframe.addEventListener('mozbrowserusernameandpasswordrequired', testHttpAuth);
    SimpleTest.executeSoon(function() {
      // Use absolute path because we need to specify host.
      iframe.src = 'http://test/tests/dom/browser-element/mochitest/file_http_401_response.sjs';
    });
  });

  is(e.detail.realm, 'http_realm');
  is(e.detail.host, 'http://test');
  e.preventDefault();

  SimpleTest.executeSoon(function() {
    e.detail.cancel();
  });
}

function testHttpAuth(e) {
  iframe.removeEventListener("mozbrowserusernameandpasswordrequired", testHttpAuth);

  // Will authenticate with correct password, prompt should not be
  // called again.
  iframe.addEventListener("mozbrowserusernameandpasswordrequired", testFail);
  iframe.addEventListener("mozbrowsertitlechange", function onTitleChange(e) {
    iframe.removeEventListener("mozbrowsertitlechange", onTitleChange);
    iframe.removeEventListener("mozbrowserusernameandpasswordrequired", testFail);
    is(e.detail, 'http auth success');
    SimpleTest.executeSoon(testFinish);
  });

  is(e.detail.realm, 'http_realm');
  is(e.detail.host, 'http://test');
  e.preventDefault();

  SimpleTest.executeSoon(function() {
    e.detail.authenticate("httpuser", "httppass");
  });
}

function testFinish() {
  // Clear login information stored in password manager.
  var authMgr = SpecialPowers.Cc['@mozilla.org/network/http-auth-manager;1']
    .getService(SpecialPowers.Ci.nsIHttpAuthManager);
  authMgr.clearAll();

  var pwmgr = SpecialPowers.Cc["@mozilla.org/login-manager;1"]
    .getService(SpecialPowers.Ci.nsILoginManager);
  pwmgr.removeAllLogins();

  SimpleTest.finish();
}

runTest();
