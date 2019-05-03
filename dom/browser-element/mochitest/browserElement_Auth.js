/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that auth prompt works.
"use strict";

/* global browserElementTestHelpers */

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

const { NetUtil } = SpecialPowers.Cu.import("resource://gre/modules/NetUtil.jsm");

function testFail(msg) {
  ok(false, JSON.stringify(msg));
}

var iframe;

function runTest() {
  iframe = document.createElement("iframe");
  iframe.setAttribute("mozbrowser", "true");
  document.body.appendChild(iframe);

  // Wait for the initial load to finish, then navigate the page, then start test
  // by loading SJS with http 401 response.
  iframe.addEventListener("mozbrowserloadend", function() {
    iframe.addEventListener("mozbrowserusernameandpasswordrequired", testHttpAuthCancel);
    SimpleTest.executeSoon(function() {
      // Use absolute path because we need to specify host.
      iframe.src = "http://test/tests/dom/browser-element/mochitest/file_http_401_response.sjs";
    });
  }, {once: true});
}

function testHttpAuthCancel(e) {
  iframe.removeEventListener("mozbrowserusernameandpasswordrequired", testHttpAuthCancel);
  // Will cancel authentication, but prompt should not be shown again. Instead,
  // we will be led to fail message
  iframe.addEventListener("mozbrowserusernameandpasswordrequired", testFail);
  iframe.addEventListener("mozbrowsertitlechange", function(f) {
    iframe.removeEventListener("mozbrowserusernameandpasswordrequired", testFail);
    is(f.detail, "http auth failed", "expected authentication to fail");
    iframe.addEventListener("mozbrowserusernameandpasswordrequired", testHttpAuth);
    SimpleTest.executeSoon(function() {
      // Use absolute path because we need to specify host.
      iframe.src = "http://test/tests/dom/browser-element/mochitest/file_http_401_response.sjs";
    });
  }, {once: true});

  is(e.detail.realm, "http_realm", "expected realm matches");
  is(e.detail.host, "http://test", "expected host matches");
  is(e.detail.path,
     "/tests/dom/browser-element/mochitest/file_http_401_response.sjs",
     "expected path matches");
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
  iframe.addEventListener("mozbrowsertitlechange", function(f) {
    iframe.removeEventListener("mozbrowserusernameandpasswordrequired", testFail);
    is(f.detail, "http auth success", "expect authentication to succeed");
    SimpleTest.executeSoon(testProxyAuth);
  }, {once: true});

  is(e.detail.realm, "http_realm", "expected realm matches");
  is(e.detail.host, "http://test", "expected host matches");
  is(e.detail.path,
     "/tests/dom/browser-element/mochitest/file_http_401_response.sjs",
     "expected path matches");
  is(e.detail.isProxy, false, "expected isProxy is false");
  e.preventDefault();

  SimpleTest.executeSoon(function() {
    e.detail.authenticate("httpuser", "httppass");
  });
}

function testProxyAuth() {
  // The testingSJS simulates the 407 proxy authentication required response
  // for proxy server, which will trigger the browser element to send prompt
  // event with proxy infomation.
  var testingSJS = "http://test/tests/dom/browser-element/mochitest/file_http_407_response.sjs";
  var mozproxy;

  function onUserNameAndPasswordRequired(e) {
    iframe.removeEventListener("mozbrowserusernameandpasswordrequired",
                               onUserNameAndPasswordRequired);
    iframe.addEventListener("mozbrowsertitlechange", function(event) {
      iframe.removeEventListener("mozbrowserusernameandpasswordrequired", testFail);
      is(event.detail, "http auth success", "expect authentication to succeed");
      SimpleTest.executeSoon(testAuthJarNoInterfere);
    }, {once: true});

    is(e.detail.realm, "http_realm", "expected realm matches");
    is(e.detail.host, mozproxy, "expected host matches");
    is(e.detail.path,
       "/tests/dom/browser-element/mochitest/file_http_407_response.sjs",
       "expected path matches");
    is(e.detail.isProxy, true, "expected isProxy is true");
    e.preventDefault();

    SimpleTest.executeSoon(function() {
      e.detail.authenticate("proxyuser", "proxypass");
    });
  }

  // Resolve proxy information used by the test suite, we need it to validate
  // whether the proxy information delivered with the prompt event is correct.
  var resolveCallback = SpecialPowers.wrapCallbackObject({
    // eslint-disable-next-line mozilla/use-chromeutils-generateqi
    QueryInterface(iid) {
      const interfaces = [Ci.nsIProtocolProxyCallback, Ci.nsISupports];
      if (!interfaces.some( function(v) { return iid.equals(v); } )) {
        throw SpecialPowers.Cr.NS_ERROR_NO_INTERFACE;
      }
      return this;
    },

    onProxyAvailable(req, channel, pi, status) {
      isnot(pi, null, "expected proxy information available");
      if (pi) {
        mozproxy = "moz-proxy://" + pi.host + ":" + pi.port;
      }
      iframe.addEventListener("mozbrowserusernameandpasswordrequired",
                              onUserNameAndPasswordRequired);

      iframe.src = testingSJS;
    },
  });

  var channel = NetUtil.newChannel({
    uri: testingSJS,
    loadUsingSystemPrincipal: true,
  });

  var pps = SpecialPowers.Cc["@mozilla.org/network/protocol-proxy-service;1"]
            .getService();

  pps.asyncResolve(channel, 0, resolveCallback);
}

function testAuthJarNoInterfere(e) {
  let authMgr = SpecialPowers.Cc["@mozilla.org/network/http-auth-manager;1"]
    .getService(SpecialPowers.Ci.nsIHttpAuthManager);
  let secMan = SpecialPowers.Services.scriptSecurityManager;
  let ioService = SpecialPowers.Services.io;
  var uri = ioService.newURI("http://test/tests/dom/browser-element/mochitest/file_http_401_response.sjs");

  // Set a bunch of auth data that should not conflict with the correct auth data already
  // stored in the cache.
  var attrs = {userContextId: 1};
  var principal = secMan.createCodebasePrincipal(uri, attrs);
  authMgr.setAuthIdentity("http", "test", -1, "basic", "http_realm",
                          "tests/dom/browser-element/mochitest/file_http_401_response.sjs",
                          "", "httpuser", "wrongpass", false, principal);
  attrs = {userContextId: 1, inIsolatedMozBrowser: true};
  principal = secMan.createCodebasePrincipal(uri, attrs);
  authMgr.setAuthIdentity("http", "test", -1, "basic", "http_realm",
                          "tests/dom/browser-element/mochitest/file_http_401_response.sjs",
                          "", "httpuser", "wrongpass", false, principal);
  principal = secMan.createCodebasePrincipal(uri, {});
  authMgr.setAuthIdentity("http", "test", -1, "basic", "http_realm",
                          "tests/dom/browser-element/mochitest/file_http_401_response.sjs",
                          "", "httpuser", "wrongpass", false, principal);

  // Will authenticate with correct password, prompt should not be
  // called again.
  iframe.addEventListener("mozbrowserusernameandpasswordrequired", testFail);
  iframe.addEventListener("mozbrowsertitlechange", function(f) {
    iframe.removeEventListener("mozbrowserusernameandpasswordrequired", testFail);
    is(f.detail, "http auth success", "expected authentication success");
    SimpleTest.executeSoon(testAuthJarInterfere);
  }, {once: true});

  // Once more with feeling. Ensure that our new auth data doesn't interfere with this mozbrowser's
  // auth data.
  iframe.src = "http://test/tests/dom/browser-element/mochitest/file_http_401_response.sjs";
}

function testAuthJarInterfere(e) {
  let authMgr = SpecialPowers.Cc["@mozilla.org/network/http-auth-manager;1"]
    .getService(SpecialPowers.Ci.nsIHttpAuthManager);
  let secMan = SpecialPowers.Services.scriptSecurityManager;
  let ioService = SpecialPowers.Services.io;
  var uri = ioService.newURI("http://test/tests/dom/browser-element/mochitest/file_http_401_response.sjs");

  // Set some auth data that should overwrite the successful stored details.
  var principal = secMan.createCodebasePrincipal(uri, {inIsolatedMozBrowser: true});
  authMgr.setAuthIdentity("http", "test", -1, "basic", "http_realm",
                          "tests/dom/browser-element/mochitest/file_http_401_response.sjs",
                          "", "httpuser", "wrongpass", false, principal);

  // Will authenticate with correct password, prompt should not be
  // called again.
  var gotusernamepasswordrequired = false;
  function onUserNameAndPasswordRequired() {
      gotusernamepasswordrequired = true;
  }
  iframe.addEventListener("mozbrowserusernameandpasswordrequired",
                          onUserNameAndPasswordRequired);
  iframe.addEventListener("mozbrowsertitlechange", function(f) {
    iframe.removeEventListener("mozbrowserusernameandpasswordrequired",
                               onUserNameAndPasswordRequired);
    ok(gotusernamepasswordrequired,
       "Should have dispatched mozbrowserusernameandpasswordrequired event");
    testFinish();
  }, {once: true});

  // Once more with feeling. Ensure that our new auth data interferes with this mozbrowser's
  // auth data.
  iframe.src = "http://test/tests/dom/browser-element/mochitest/file_http_401_response.sjs";
}

function testFinish() {
  // Clear login information stored in password manager.
  let authMgr = SpecialPowers.Cc["@mozilla.org/network/http-auth-manager;1"]
    .getService(SpecialPowers.Ci.nsIHttpAuthManager);
  authMgr.clearAll();

  SpecialPowers.Services.logins.removeAllLogins();

  SimpleTest.finish();
}

addEventListener("testready", function() {
  // Enable http authentiication.
  SpecialPowers.pushPrefEnv({"set": [["network.auth.non-web-content-triggered-resources-http-auth-allow", true]]}, runTest);
});
