/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests that security info parser gives correct general security state for
// different cases.

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});

Object.defineProperty(this, "NetworkHelper", {
  get: function() {
    return require("devtools/shared/webconsole/network-helper");
  },
  configurable: true,
  writeable: false,
  enumerable: true
});

const wpl = Ci.nsIWebProgressListener;
const MockSecurityInfo = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsITransportSecurityInfo,
                                          Ci.nsISSLStatusProvider]),
  securityState: wpl.STATE_IS_BROKEN,
  errorCode: 0,
  SSLStatus: {
    // nsISSLStatus.TLS_VERSION_1_2
    protocolVersion: 3,
    cipherSuite: "TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256",
  }
};

function run_test() {
  test_nullSecurityInfo();
  test_insecureSecurityInfoWithNSSError();
  test_insecureSecurityInfoWithoutNSSError();
  test_brokenSecurityInfo();
  test_secureSecurityInfo();
}

/**
 * Test that undefined security information is returns "insecure".
 */
function test_nullSecurityInfo() {
  let result = NetworkHelper.parseSecurityInfo(null, {});
  equal(result.state, "insecure",
    "state == 'insecure' when securityInfo was undefined");
}

/**
 * Test that STATE_IS_INSECURE with NSSError returns "broken"
 */
function test_insecureSecurityInfoWithNSSError() {
  MockSecurityInfo.securityState = wpl.STATE_IS_INSECURE;

  // Taken from security/manager/ssl/tests/unit/head_psm.js.
  MockSecurityInfo.errorCode = -8180;

  let result = NetworkHelper.parseSecurityInfo(MockSecurityInfo, {});
  equal(result.state, "broken",
    "state == 'broken' if securityState contains STATE_IS_INSECURE flag AND " +
    "errorCode is NSS error.");

  MockSecurityInfo.errorCode = 0;
}

/**
 * Test that STATE_IS_INSECURE without NSSError returns "insecure"
 */
function test_insecureSecurityInfoWithoutNSSError() {
  MockSecurityInfo.securityState = wpl.STATE_IS_INSECURE;

  let result = NetworkHelper.parseSecurityInfo(MockSecurityInfo, {});
  equal(result.state, "insecure",
    "state == 'insecure' if securityState contains STATE_IS_INSECURE flag BUT " +
    "errorCode is not NSS error.");
}

/**
 * Test that STATE_IS_SECURE returns "secure"
 */
function test_secureSecurityInfo() {
  MockSecurityInfo.securityState = wpl.STATE_IS_SECURE;

  let result = NetworkHelper.parseSecurityInfo(MockSecurityInfo, {});
  equal(result.state, "secure",
    "state == 'secure' if securityState contains STATE_IS_SECURE flag");
}

/**
 * Test that STATE_IS_BROKEN returns "weak"
 */
function test_brokenSecurityInfo() {
  MockSecurityInfo.securityState = wpl.STATE_IS_BROKEN;

  let result = NetworkHelper.parseSecurityInfo(MockSecurityInfo, {});
  equal(result.state, "weak",
    "state == 'weak' if securityState contains STATE_IS_BROKEN flag");
}
