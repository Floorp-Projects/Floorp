/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that auth prompt works.
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function testFail(msg) {
  ok(false, JSON.stringify(msg));
}

var iframe;

function runTest() {
  iframe = document.createElement('iframe');
  SpecialPowers.wrap(iframe).mozbrowser = true;
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
    is(e.detail, 'http auth failed', 'expected authentication to fail');
    iframe.addEventListener('mozbrowserusernameandpasswordrequired', testHttpAuth);
    SimpleTest.executeSoon(function() {
      // Use absolute path because we need to specify host.
      iframe.src = 'http://test/tests/dom/browser-element/mochitest/file_http_401_response.sjs';
    });
  });

  is(e.detail.realm, 'http_realm', 'expected realm matches');
  is(e.detail.host, 'http://test', 'expected host matches');
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
    is(e.detail, 'http auth success', 'expect authentication to succeed');
    SimpleTest.executeSoon(testAuthJarNoInterfere);
  });

  is(e.detail.realm, 'http_realm', 'expected realm matches');
  is(e.detail.host, 'http://test', 'expected host matches');
  e.preventDefault();

  SimpleTest.executeSoon(function() {
    e.detail.authenticate("httpuser", "httppass");
  });
}

function testAuthJarNoInterfere(e) {
  var authMgr = SpecialPowers.Cc['@mozilla.org/network/http-auth-manager;1']
    .getService(SpecialPowers.Ci.nsIHttpAuthManager);
  var secMan = SpecialPowers.Cc["@mozilla.org/scriptsecuritymanager;1"]
               .getService(SpecialPowers.Ci.nsIScriptSecurityManager);
  var ioService = SpecialPowers.Cc["@mozilla.org/network/io-service;1"]
                  .getService(SpecialPowers.Ci.nsIIOService);
  var uri = ioService.newURI("http://test/tests/dom/browser-element/mochitest/file_http_401_response.sjs", null, null);

  // Set a bunch of auth data that should not conflict with the correct auth data already
  // stored in the cache.
  var principal = secMan.getAppCodebasePrincipal(uri, 1, false);
  authMgr.setAuthIdentity('http', 'test', -1, 'basic', 'http_realm',
                          'tests/dom/browser-element/mochitest/file_http_401_response.sjs',
                          '', 'httpuser', 'wrongpass', false, principal);
  principal = secMan.getAppCodebasePrincipal(uri, 1, true);
  authMgr.setAuthIdentity('http', 'test', -1, 'basic', 'http_realm',
                          'tests/dom/browser-element/mochitest/file_http_401_response.sjs',
                          '', 'httpuser', 'wrongpass', false, principal);
  principal = secMan.getAppCodebasePrincipal(uri, secMan.NO_APP_ID, false);
  authMgr.setAuthIdentity('http', 'test', -1, 'basic', 'http_realm',
                          'tests/dom/browser-element/mochitest/file_http_401_response.sjs',
                          '', 'httpuser', 'wrongpass', false, principal);

  // Will authenticate with correct password, prompt should not be
  // called again.
  iframe.addEventListener("mozbrowserusernameandpasswordrequired", testFail);
  iframe.addEventListener("mozbrowsertitlechange", function onTitleChange(e) {
    iframe.removeEventListener("mozbrowsertitlechange", onTitleChange);
    iframe.removeEventListener("mozbrowserusernameandpasswordrequired", testFail);
    is(e.detail, 'http auth success', 'expected authentication success');
    SimpleTest.executeSoon(testAuthJarInterfere);
  });

  // Once more with feeling. Ensure that our new auth data doesn't interfere with this mozbrowser's
  // auth data.
  iframe.src = 'http://test/tests/dom/browser-element/mochitest/file_http_401_response.sjs';
}

function testAuthJarInterfere(e) {
  var authMgr = SpecialPowers.Cc['@mozilla.org/network/http-auth-manager;1']
    .getService(SpecialPowers.Ci.nsIHttpAuthManager);
  var secMan = SpecialPowers.Cc["@mozilla.org/scriptsecuritymanager;1"]
               .getService(SpecialPowers.Ci.nsIScriptSecurityManager);
  var ioService = SpecialPowers.Cc["@mozilla.org/network/io-service;1"]
                  .getService(SpecialPowers.Ci.nsIIOService);
  var uri = ioService.newURI("http://test/tests/dom/browser-element/mochitest/file_http_401_response.sjs", null, null);

  // Set some auth data that should overwrite the successful stored details.
  var principal = secMan.getAppCodebasePrincipal(uri, secMan.NO_APP_ID, true);
  authMgr.setAuthIdentity('http', 'test', -1, 'basic', 'http_realm',
                          'tests/dom/browser-element/mochitest/file_http_401_response.sjs',
                          '', 'httpuser', 'wrongpass', false, principal);

  // Will authenticate with correct password, prompt should not be
  // called again.
  var gotusernamepasswordrequired = false;
  function onUserNameAndPasswordRequired() {
      gotusernamepasswordrequired = true;
  }
  iframe.addEventListener("mozbrowserusernameandpasswordrequired",
                          onUserNameAndPasswordRequired);
  iframe.addEventListener("mozbrowsertitlechange", function onTitleChange(e) {
    iframe.removeEventListener("mozbrowsertitlechange", onTitleChange);
    iframe.removeEventListener("mozbrowserusernameandpasswordrequired",
                               onUserNameAndPasswordRequired);
    ok(gotusernamepasswordrequired,
       "Should have dispatched mozbrowserusernameandpasswordrequired event");
    testFinish();
  });

  // Once more with feeling. Ensure that our new auth data interferes with this mozbrowser's
  // auth data.
  iframe.src = 'http://test/tests/dom/browser-element/mochitest/file_http_401_response.sjs';
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

addEventListener('testready', runTest);
