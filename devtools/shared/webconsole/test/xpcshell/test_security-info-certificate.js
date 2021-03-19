/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests that NetworkHelper.parseCertificateInfo parses certificate information
// correctly.

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");

Object.defineProperty(this, "NetworkHelper", {
  get: function() {
    return require("devtools/shared/webconsole/network-helper");
  },
  configurable: true,
  writeable: false,
  enumerable: true,
});

const DUMMY_CERT = {
  getBase64DERString: function() {
    // This is the base64-encoded contents of the "DigiCert ECC Secure Server CA"
    // intermediate certificate as issued by "DigiCert Global Root CA". It was
    // chosen as a test certificate because it has an issuer common name,
    // organization, and organizational unit that are somewhat distinct from
    // its subject common name and organization name.
    return "MIIDrDCCApSgAwIBAgIQCssoukZe5TkIdnRw883GEjANBgkqhkiG9w0BAQwFADBhMQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBDQTAeFw0xMzAzMDgxMjAwMDBaFw0yMzAzMDgxMjAwMDBaMEwxCzAJBgNVBAYTAlVTMRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxJjAkBgNVBAMTHURpZ2lDZXJ0IEVDQyBTZWN1cmUgU2VydmVyIENBMHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE4ghC6nfYJN6gLGSkE85AnCNyqQIKDjc/ITa4jVMU9tWRlUvzlgKNcR7E2Munn17voOZ/WpIRllNv68DLP679Wz9HJOeaBy6Wvqgvu1cYr3GkvXg6HuhbPGtkESvMNCuMo4IBITCCAR0wEgYDVR0TAQH/BAgwBgEB/wIBADAOBgNVHQ8BAf8EBAMCAYYwNAYIKwYBBQUHAQEEKDAmMCQGCCsGAQUFBzABhhhodHRwOi8vb2NzcC5kaWdpY2VydC5jb20wQgYDVR0fBDswOTA3oDWgM4YxaHR0cDovL2NybDMuZGlnaWNlcnQuY29tL0RpZ2lDZXJ0R2xvYmFsUm9vdENBLmNybDA9BgNVHSAENjA0MDIGBFUdIAAwKjAoBggrBgEFBQcCARYcaHR0cHM6Ly93d3cuZGlnaWNlcnQuY29tL0NQUzAdBgNVHQ4EFgQUo53mH/naOU/AbuiRy5Wl2jHiCp8wHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUwDQYJKoZIhvcNAQEMBQADggEBAMeKoENL7HTJxavVHzA1Nm6YVntIrAVjrnuaVyRXzG/63qttnMe2uuzO58pzZNvfBDcKAEmzP58mrZGMIOgfiA4q+2Y3yDDo0sIkp0VILeoBUEoxlBPfjV/aKrtJPGHzecicZpIalir0ezZYoyxBEHQa0+1IttK7igZFcTMQMHp6mCHdJLnsnLWSB62DxsRq+HfmNb4TDydkskO/g+l3VtsIh5RHFPVfKK+jaEyDj2D3loB5hWp2Jp2VDCADjT7ueihlZGak2YPqmXTNbk19HOuNssWvFhtOyPNV6og4ETQdEa8/B6hPatJ0ES8q/HO3X8IVQwVs1n3aAr0im0/T+Xc=";
  },
};

add_task(async function run_test() {
  info("Testing NetworkHelper.parseCertificateInfo.");

  const result = await NetworkHelper.parseCertificateInfo(DUMMY_CERT);

  // Subject
  equal(
    result.subject.commonName,
    "DigiCert ECC Secure Server CA",
    "Common name is correct."
  );
  equal(
    result.subject.organization,
    "DigiCert Inc",
    "Organization is correct."
  );
  equal(
    result.subject.organizationUnit,
    undefined,
    "Organizational unit is correct."
  );

  // Issuer
  equal(
    result.issuer.commonName,
    "DigiCert Global Root CA",
    "Common name of the issuer is correct."
  );
  equal(
    result.issuer.organization,
    "DigiCert Inc",
    "Organization of the issuer is correct."
  );
  equal(
    result.issuer.organizationUnit,
    "www.digicert.com",
    "Organizational unit of the issuer is correct."
  );

  // Validity
  equal(
    result.validity.start,
    "Fri, 08 Mar 2013 12:00:00 GMT",
    "Start of the validity period is correct."
  );
  equal(
    result.validity.end,
    "Wed, 08 Mar 2023 12:00:00 GMT",
    "End of the validity period is correct."
  );

  // Fingerprints
  equal(
    result.fingerprint.sha1,
    "56:EE:7C:27:06:83:16:2D:83:BA:EA:CC:79:0E:22:47:1A:DA:AB:E8",
    "Certificate SHA1 fingerprint is correct."
  );
  equal(
    result.fingerprint.sha256,
    "45:84:46:BA:75:D9:32:E9:14:F2:3C:2B:57:B7:D1:92:ED:DB:C2:18:1D:95:8E:11:81:AD:52:51:74:7A:1E:E8",
    "Certificate SHA256 fingerprint is correct."
  );
});
