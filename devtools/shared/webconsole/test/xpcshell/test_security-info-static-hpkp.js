/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that NetworkHelper.parseSecurityInfo correctly detects static hpkp pins

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
  securityState: wpl.STATE_IS_SECURE,
  errorCode: 0,
  cipherName: "TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256",
  // TLS_VERSION_1_2
  protocolVersion: 3,
  serverCert: {
    getBase64DERString() {
      // This is the same test certificate as in
      // test_security-info-certificate.js for consistency.
      return "MIIDrDCCApSgAwIBAgIQCssoukZe5TkIdnRw883GEjANBgkqhkiG9w0BAQwFADBhMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBDQTAeFw0xMzAzMDgxMjAwMDBaFw0yMzAzMDgxMjAwMDBaMEwxCzAJBgNVBAYTAlVTMRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxJjAkBgNVBAMTHURpZ2lDZXJ0IEVDQyBTZWN1cmUgU2VydmVyIENBMHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE4ghC6nfYJN6gLGSkE85AnCNyqQIKDjc/ITa4jVMU9tWRlUvzlgKNcR7E2Munn17voOZ/WpIRllNv68DLP679Wz9HJOeaBy6Wvqgvu1cYr3GkvXg6HuhbPGtkESvMNCuMo4IBITCCAR0wEgYDVR0TAQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8EBAMCAYYwNAYIKwYBBQUHAQEEKDAmMCQGCCsGAQUFBzABhhhodHRwOi8vb2NzcC5kaWdpY2VydC5jb20wQgYDVR0fBDswOTA3oDWgM4YxaHR0cDovL2NybDMuZGlnaWNlcnQuY29tL0RpZ2lDZXJ0R2xvYmFsUm9vdENBLmNybDA9BgNVHSAENjA0MDIGBFUdIAAwKjAoBggrBgEFBQcCARYcaHR0cHM6Ly93d3cuZGlnaWNlcnQuY29tL0NQUzAdBgNVHQ4EFgQUo53mH/naOU/AbuiRy5Wl2jHiCp8wHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUwDQYJKoZIhvcNAQEMBQADggEBAMeKoENL7HTJxavVHzA1Nm6YVntIrAVjrnuaVyRXzG/63qttnMe2uuzO58pzZNvfBDcKAEmzP58mrZGMIOgfiA4q+2Y3yDDo0sIkp0VILeoBUEoxlBPfjV/aKrtJPGHzecicZpIalir0ezZYoyxBEHQa0+1IttK7igZFcTMQMHp6mCHdJLnsnLWSB62DxsRq+HfmNb4TDydkskO/g+l3VtsIh5RHFPVfKK+jaEyDj2D3loB5hWp2Jp2VDCADjT7ueihlZGak2YPqmXTNbk19HOuNssWvFhtOyPNV6og4ETQdEa8/B6hPatJ0ES8q/HO3X8IVQwVs1n3aAr0im0/T+Xc=";
    },
  },
};

const MockHttpInfo = {
  hostname: "include-subdomains.pinning.example.com",
  private: false,
};

add_task(async function run_test() {
  Services.prefs.setIntPref("security.cert_pinning.enforcement_level", 1);
  const result = await NetworkHelper.parseSecurityInfo(
    MockSecurityInfo,
    {},
    MockHttpInfo,
    new Map()
  );
  equal(result.hpkp, true, "Static HPKP detected.");
});
