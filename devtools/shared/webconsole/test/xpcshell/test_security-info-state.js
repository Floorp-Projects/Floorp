/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests that security info parser gives correct general security state for
// different cases.

Object.defineProperty(this, "NetworkHelper", {
  get() {
    return require("devtools/shared/webconsole/network-helper");
  },
  configurable: true,
  writeable: false,
  enumerable: true,
});

const wpl = Ci.nsIWebProgressListener;

// This *cannot* be used as an nsITransportSecurityInfo (since that interface is
// builtinclass) but the methods being tested aren't defined by XPCOM and aren't
// calling QueryInterface, so this usage is fine.
const MockSecurityInfo = {
  securityState: wpl.STATE_IS_BROKEN,
  errorCode: 0,
  // nsISSLStatus.TLS_VERSION_1_2
  protocolVersion: 3,
  cipherName: "TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256",
};

add_task(async function run_test() {
  await test_nullSecurityInfo();
  await test_insecureSecurityInfoWithNSSError();
  await test_insecureSecurityInfoWithoutNSSError();
  await test_brokenSecurityInfo();
  await test_secureSecurityInfo();
});

/**
 * Test that undefined security information is returns "insecure".
 */
async function test_nullSecurityInfo() {
  const result = await NetworkHelper.parseSecurityInfo(null, {}, {}, new Map());
  equal(
    result.state,
    "insecure",
    "state == 'insecure' when securityInfo was undefined"
  );
}

/**
 * Test that STATE_IS_INSECURE with NSSError returns "broken"
 */
async function test_insecureSecurityInfoWithNSSError() {
  MockSecurityInfo.securityState = wpl.STATE_IS_INSECURE;

  // Taken from security/manager/ssl/tests/unit/head_psm.js.
  MockSecurityInfo.errorCode = -8180;

  const result = await NetworkHelper.parseSecurityInfo(
    MockSecurityInfo,
    {},
    {},
    new Map()
  );
  equal(
    result.state,
    "broken",
    "state == 'broken' if securityState contains STATE_IS_INSECURE flag AND " +
      "errorCode is NSS error."
  );

  MockSecurityInfo.errorCode = 0;
}

/**
 * Test that STATE_IS_INSECURE without NSSError returns "insecure"
 */
async function test_insecureSecurityInfoWithoutNSSError() {
  MockSecurityInfo.securityState = wpl.STATE_IS_INSECURE;

  const result = await NetworkHelper.parseSecurityInfo(
    MockSecurityInfo,
    {},
    {},
    new Map()
  );
  equal(
    result.state,
    "insecure",
    "state == 'insecure' if securityState contains STATE_IS_INSECURE flag BUT " +
      "errorCode is not NSS error."
  );
}

/**
 * Test that STATE_IS_SECURE returns "secure"
 */
async function test_secureSecurityInfo() {
  MockSecurityInfo.securityState = wpl.STATE_IS_SECURE;

  const result = await NetworkHelper.parseSecurityInfo(
    MockSecurityInfo,
    {},
    {},
    new Map()
  );
  equal(
    result.state,
    "secure",
    "state == 'secure' if securityState contains STATE_IS_SECURE flag"
  );
}

/**
 * Test that STATE_IS_BROKEN returns "weak"
 */
async function test_brokenSecurityInfo() {
  MockSecurityInfo.securityState = wpl.STATE_IS_BROKEN;

  const result = await NetworkHelper.parseSecurityInfo(
    MockSecurityInfo,
    {},
    {},
    new Map()
  );
  equal(
    result.state,
    "weak",
    "state == 'weak' if securityState contains STATE_IS_BROKEN flag"
  );
}
