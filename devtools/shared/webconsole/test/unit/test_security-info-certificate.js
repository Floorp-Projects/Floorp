/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Tests that NetworkHelper.parseCertificateInfo parses certificate information
// correctly.

const { require } = Components.utils.import("resource://devtools/shared/Loader.jsm", {});

Object.defineProperty(this, "NetworkHelper", {
  get: function() {
    return require("devtools/shared/webconsole/network-helper");
  },
  configurable: true,
  writeable: false,
  enumerable: true
});

var Ci = Components.interfaces;
const DUMMY_CERT = {
  commonName: "cn",
  organization: "o",
  organizationalUnit: "ou",
  issuerCommonName: "issuerCN",
  issuerOrganization: "issuerO",
  issuerOrganizationUnit: "issuerOU",
  sha256Fingerprint: "qwertyuiopoiuytrewq",
  sha1Fingerprint: "qwertyuiop",
  validity: {
    notBeforeLocalDay: "yesterday",
    notAfterLocalDay: "tomorrow",
  }
};

function run_test() {
  do_print("Testing NetworkHelper.parseCertificateInfo.");

  let result = NetworkHelper.parseCertificateInfo(DUMMY_CERT);

  // Subject
  equal(result.subject.commonName, DUMMY_CERT.commonName,
    "Common name is correct.");
  equal(result.subject.organization, DUMMY_CERT.organization,
    "Organization is correct.");
  equal(result.subject.organizationalUnit, DUMMY_CERT.organizationalUnit,
    "Organizational unit is correct.");

  // Issuer
  equal(result.issuer.commonName, DUMMY_CERT.issuerCommonName,
    "Common name of the issuer is correct.");
  equal(result.issuer.organization, DUMMY_CERT.issuerOrganization,
    "Organization of the issuer is correct.");
  equal(result.issuer.organizationalUnit, DUMMY_CERT.issuerOrganizationalUnit,
    "Organizational unit of the issuer is correct.");

  // Validity
  equal(result.validity.start, DUMMY_CERT.validity.notBeforeLocalDay,
    "Start of the validity period is correct.");
  equal(result.validity.end, DUMMY_CERT.validity.notAfterLocalDay,
    "End of the validity period is correct.");

  // Fingerprints
  equal(result.fingerprint.sha1, DUMMY_CERT.sha1Fingerprint,
    "Certificate SHA1 fingerprint is correct.");
  equal(result.fingerprint.sha256, DUMMY_CERT.sha256Fingerprint,
    "Certificate SHA256 fingerprint is correct.");
}
