/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that NetworkHelper.parseSecurityInfo correctly detects static hpkp pins

const { require } = Components.utils.import("resource://gre/modules/devtools/shared/Loader.jsm", {});
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

Object.defineProperty(this, "NetworkHelper", {
  get: function() {
    return require("devtools/shared/webconsole/network-helper");
  },
  configurable: true,
  writeable: false,
  enumerable: true
});

const Ci = Components.interfaces;
const wpl = Ci.nsIWebProgressListener;

const MockSecurityInfo = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITransportSecurityInfo,
                                         Ci.nsISSLStatusProvider]),
  securityState: wpl.STATE_IS_SECURE,
  errorCode: 0,
  SSLStatus: {
    cipherSuite: "TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256",
    protocolVersion: 3, // TLS_VERSION_1_2
    serverCert: {
      validity: {}
    },
  }
};

const MockHttpInfo = {
  hostname: "include-subdomains.pinning.example.com",
  private: false,
};

function run_test() {
  Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 1);
  let result = NetworkHelper.parseSecurityInfo(MockSecurityInfo, MockHttpInfo);
  equal(result.hpkp, true, "Static HPKP detected.");
}
