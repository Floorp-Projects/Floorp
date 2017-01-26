/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/UpdateUtils.jsm");
Cu.import("resource://gre/modules/TelemetryController.jsm");

 // Percentage of the population to attempt to disable SHA-1 for, by channel.
const TEST_THRESHOLD = {
  beta: 0.1, // 10%
};

const PREF_COHORT_SAMPLE = "disableSHA1.rollout.cohortSample";
const PREF_COHORT_NAME = "disableSHA1.rollout.cohort";
const PREF_SHA1_POLICY = "security.pki.sha1_enforcement_level";
const PREF_SHA1_POLICY_SET_BY_ADDON = "disableSHA1.rollout.policySetByAddOn";
const PREF_SHA1_POLICY_RESET_BY_USER = "disableSHA1.rollout.userResetPref";

const SHA1_MODE_ALLOW = 0;
const SHA1_MODE_FORBID = 1;
const SHA1_MODE_IMPORTED_ROOTS_ONLY = 3;
const SHA1_MODE_CURRENT_DEFAULT = 4;

function startup() {
  Preferences.observe(PREF_SHA1_POLICY, policyPreferenceChanged);
}

function install() {
  let updateChannel = UpdateUtils.getUpdateChannel(false);
  if (updateChannel in TEST_THRESHOLD) {
    makeRequest().then(defineCohort).catch((e) => console.error(e));
  }
}

function policyPreferenceChanged() {
  let currentPrefValue = Preferences.get(PREF_SHA1_POLICY,
                                         SHA1_MODE_CURRENT_DEFAULT);
  Preferences.reset(PREF_SHA1_POLICY_RESET_BY_USER);
  if (currentPrefValue == SHA1_MODE_CURRENT_DEFAULT) {
    Preferences.set(PREF_SHA1_POLICY_RESET_BY_USER, true);
  }
}

function defineCohort(result) {
  let userOptedOut = optedOut();
  let userOptedIn = optedIn();
  let shouldNotDisableSHA1Because = reasonToNotDisableSHA1(result);
  let safeToDisableSHA1 = shouldNotDisableSHA1Because.length == 0;
  let updateChannel = UpdateUtils.getUpdateChannel(false);
  let testGroup = getUserSample() < TEST_THRESHOLD[updateChannel];

  let cohortName;
  if (!safeToDisableSHA1) {
    cohortName = "notSafeToDisableSHA1";
  } else if (userOptedOut) {
    cohortName = "optedOut";
  } else if (userOptedIn) {
    cohortName = "optedIn";
  } else if (testGroup) {
    cohortName = "test";
    Preferences.ignore(PREF_SHA1_POLICY, policyPreferenceChanged);
    Preferences.set(PREF_SHA1_POLICY, SHA1_MODE_IMPORTED_ROOTS_ONLY);
    Preferences.observe(PREF_SHA1_POLICY, policyPreferenceChanged);
    Preferences.set(PREF_SHA1_POLICY_SET_BY_ADDON, true);
  } else {
    cohortName = "control";
  }
  Preferences.set(PREF_COHORT_NAME, cohortName);
  reportTelemetry(result, cohortName, shouldNotDisableSHA1Because,
                  cohortName == "test");
}

function shutdown(data, reason) {
  Preferences.ignore(PREF_SHA1_POLICY, policyPreferenceChanged);
}

function uninstall() {
}

function getUserSample() {
  let prefValue = Preferences.get(PREF_COHORT_SAMPLE, undefined);
  let value = 0.0;

  if (typeof(prefValue) == "string") {
    value = parseFloat(prefValue, 10);
    return value;
  }

  value = Math.random();

  Preferences.set(PREF_COHORT_SAMPLE, value.toString().substr(0, 8));
  return value;
}

function reportTelemetry(result, cohortName, didNotDisableSHA1Because,
                         disabledSHA1) {
  result.cohortName = cohortName;
  result.disabledSHA1 = disabledSHA1;
  if (cohortName == "optedOut") {
    let userResetPref = Preferences.get(PREF_SHA1_POLICY_RESET_BY_USER, false);
    let currentPrefValue = Preferences.get(PREF_SHA1_POLICY,
                                           SHA1_MODE_CURRENT_DEFAULT);
    if (userResetPref) {
      didNotDisableSHA1Because = "preference:userReset";
    } else if (currentPrefValue == SHA1_MODE_ALLOW) {
      didNotDisableSHA1Because = "preference:allow";
    } else {
      didNotDisableSHA1Because = "preference:forbid";
    }
  }
  result.didNotDisableSHA1Because = didNotDisableSHA1Because;
  return TelemetryController.submitExternalPing("disableSHA1rollout", result,
                                                {});
}

function optedIn() {
  let policySetByAddOn = Preferences.get(PREF_SHA1_POLICY_SET_BY_ADDON, false);
  let currentPrefValue = Preferences.get(PREF_SHA1_POLICY,
                                         SHA1_MODE_CURRENT_DEFAULT);
  return currentPrefValue == SHA1_MODE_IMPORTED_ROOTS_ONLY && !policySetByAddOn;
}

function optedOut() {
  // Users can also opt-out by setting the policy to always allow or always
  // forbid SHA-1, or by resetting the preference after this add-on has changed
  // it (in that case, this will be reported the next time this add-on is
  // updated).
  let currentPrefValue = Preferences.get(PREF_SHA1_POLICY,
                                         SHA1_MODE_CURRENT_DEFAULT);
  let userResetPref = Preferences.get(PREF_SHA1_POLICY_RESET_BY_USER, false);
  return currentPrefValue == SHA1_MODE_ALLOW ||
         currentPrefValue == SHA1_MODE_FORBID ||
         userResetPref;
}

function delocalizeAlgorithm(localizedString) {
  let bundle = Services.strings.createBundle(
    "chrome://pipnss/locale/pipnss.properties");
  let algorithmStringIdsToOIDDescriptionMap = {
    "CertDumpMD2WithRSA":                       "md2WithRSAEncryption",
    "CertDumpMD5WithRSA":                       "md5WithRSAEncryption",
    "CertDumpSHA1WithRSA":                      "sha1WithRSAEncryption",
    "CertDumpSHA256WithRSA":                    "sha256WithRSAEncryption",
    "CertDumpSHA384WithRSA":                    "sha384WithRSAEncryption",
    "CertDumpSHA512WithRSA":                    "sha512WithRSAEncryption",
    "CertDumpAnsiX962ECDsaSignatureWithSha1":   "ecdsaWithSHA1",
    "CertDumpAnsiX962ECDsaSignatureWithSha224": "ecdsaWithSHA224",
    "CertDumpAnsiX962ECDsaSignatureWithSha256": "ecdsaWithSHA256",
    "CertDumpAnsiX962ECDsaSignatureWithSha384": "ecdsaWithSHA384",
    "CertDumpAnsiX962ECDsaSignatureWithSha512": "ecdsaWithSHA512",
  };

  let description;
  Object.keys(algorithmStringIdsToOIDDescriptionMap).forEach((l10nID) => {
    let candidateLocalizedString = bundle.GetStringFromName(l10nID);
    if (localizedString == candidateLocalizedString) {
      description = algorithmStringIdsToOIDDescriptionMap[l10nID];
    }
  });
  if (!description) {
    return "unknown";
  }
  return description;
}

function getSignatureAlgorithm(cert) {
  // Certificate  ::=  SEQUENCE  {
  //      tbsCertificate       TBSCertificate,
  //      signatureAlgorithm   AlgorithmIdentifier,
  //      signatureValue       BIT STRING  }
  let certificate = cert.ASN1Structure.QueryInterface(Ci.nsIASN1Sequence);
  let signatureAlgorithm = certificate.ASN1Objects
                                      .queryElementAt(1, Ci.nsIASN1Sequence);
  // AlgorithmIdentifier  ::=  SEQUENCE  {
  //      algorithm               OBJECT IDENTIFIER,
  //      parameters              ANY DEFINED BY algorithm OPTIONAL  }

  // If parameters is NULL (or empty), signatureAlgorithm won't be a container
  // under this implementation. Just get its displayValue.
  if (!signatureAlgorithm.isValidContainer) {
    return signatureAlgorithm.displayValue;
  }
  let oid = signatureAlgorithm.ASN1Objects.queryElementAt(0, Ci.nsIASN1Object);
  return oid.displayValue;
}

function processCertChain(chain) {
  let output = [];
  let enumerator = chain.getEnumerator();
  while (enumerator.hasMoreElements()) {
    let cert = enumerator.getNext().QueryInterface(Ci.nsIX509Cert);
    output.push({
      sha256Fingerprint: cert.sha256Fingerprint.replace(/:/g, "").toLowerCase(),
      isBuiltInRoot: cert.isBuiltInRoot,
      signatureAlgorithm: delocalizeAlgorithm(getSignatureAlgorithm(cert)),
    });
  }
  return output;
}

class CertificateVerificationResult {
  constructor(resolve) {
    this.resolve = resolve;
  }

  verifyCertFinished(aPRErrorCode, aVerifiedChain, aEVStatus) {
    let result = { errorCode: aPRErrorCode, error: "", chain: [] };
    if (aPRErrorCode == 0) {
      result.chain = processCertChain(aVerifiedChain);
    } else {
      result.error = "certificate reverification";
    }
    this.resolve(result);
  }
}

function makeRequest() {
  return new Promise((resolve) => {
    let hostname = "telemetry.mozilla.org";
    let req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                .createInstance(Ci.nsIXMLHttpRequest);
    req.open("GET", "https://" + hostname);
    req.channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
    req.timeout = 30000;
    req.addEventListener("error", (evt) => {
      // If we can't connect to telemetry.mozilla.org, then how did we even
      // download the experiment? In any case, we may still be able to get some
      // information.
      let result = { error: "connection error" };
      if (evt.target.channel && evt.target.channel.securityInfo) {
        let securityInfo = evt.target.channel.securityInfo
                             .QueryInterface(Ci.nsITransportSecurityInfo);
        if (securityInfo) {
          result.errorCode = securityInfo.errorCode;
        }
        if (securityInfo && securityInfo.failedCertChain) {
          result.chain = processCertChain(securityInfo.failedCertChain);
        }
      }
      resolve(result);
    });
    req.addEventListener("timeout", (evt) => {
      resolve({ error: "timeout" });
    });
    req.addEventListener("load", (evt) => {
      let securityInfo = evt.target.channel.securityInfo
                           .QueryInterface(Ci.nsITransportSecurityInfo);
      if (securityInfo.securityState &
          Ci.nsIWebProgressListener.STATE_CERT_USER_OVERRIDDEN) {
        resolve({ error: "user override" });
        return;
      }
      let sslStatus = securityInfo.QueryInterface(Ci.nsISSLStatusProvider)
                        .SSLStatus;
      let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                     .getService(Ci.nsIX509CertDB);
      let result = new CertificateVerificationResult(resolve);
      // Unfortunately, we don't have direct access to the verified certificate
      // chain as built by the AuthCertificate hook, so we have to re-build it
      // here. In theory we are likely to get the same result.
      certdb.asyncVerifyCertAtTime(sslStatus.serverCert,
                                   2, // certificateUsageSSLServer
                                   0, // flags
                                   hostname,
                                   Date.now() / 1000,
                                   result);
    });
    req.send();
  });
}

// As best we know, it is safe to disable SHA1 if the connection was successful
// and either the connection was MITM'd by a root not in the public PKI or the
// chain is part of the public PKI and is the one served by the real
// telemetry.mozilla.org.
// This will return a short string description of why it might not be safe to
// disable SHA1 or an empty string if it is safe to disable SHA1.
function reasonToNotDisableSHA1(result) {
  if (!("errorCode" in result) || result.errorCode != 0) {
    return "connection error";
  }
  if (!("chain" in result)) {
    return "code error";
  }
  if (!result.chain[result.chain.length - 1].isBuiltInRoot) {
    return "";
  }
  if (result.chain.length != 3) {
    return "MITM";
  }
  const kEndEntityFingerprint = "197feaf3faa0f0ad637a89c97cb91336bfc114b6b3018203cbd9c3d10c7fa86c";
  const kIntermediateFingerprint = "154c433c491929c5ef686e838e323664a00e6a0d822ccc958fb4dab03e49a08f";
  const kRootFingerprint = "4348a0e9444c78cb265e058d5e8944b4d84f9662bd26db257f8934a443c70161";
  if (result.chain[0].sha256Fingerprint != kEndEntityFingerprint ||
      result.chain[1].sha256Fingerprint != kIntermediateFingerprint ||
      result.chain[2].sha256Fingerprint != kRootFingerprint) {
    return "MITM";
  }
  return "";
}
