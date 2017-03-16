"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/TelemetryController.jsm");
var btoa = Cu.import("resource://gre/modules/Log.jsm").btoa;

function certToBase64(cert) {
  let derString = "";
  for (let rawByte of cert.getRawDER({})) {
    derString += String.fromCharCode(rawByte);
  }
  return btoa(derString);
}

function certArrayToBase64(certs) {
  let result = [];
  for (let cert of certs) {
    result.push(certToBase64(cert));
  }
  return result;
}

function certListToJSArray(certList) {
  let result = [];
  let enumerator = certList.getEnumerator();
  while (enumerator.hasMoreElements()) {
    let cert = enumerator.getNext().QueryInterface(Ci.nsIX509Cert);
    result.push(cert);
  }
  return result;
}

class CertificateVerificationResult {
  constructor(hostname, resolve) {
    this.hostname = hostname;
    this.resolve = resolve;
  }

  verifyCertFinished(aPRErrorCode, aVerifiedChain, aEVStatus) {
    let result = { hostname: this.hostname };
    if (aPRErrorCode == 0) {
      result.chain = certListToJSArray(aVerifiedChain);
    } else {
      result.error = "certificate reverification";
      console.log(`${this.hostname}: ${aPRErrorCode}`);
    }
    this.resolve(result);
  }
}

function makeRequest(hostname) {
  return new Promise((resolve) => {
    let req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                .createInstance(Ci.nsIXMLHttpRequest);
    req.open("GET", "https://" + hostname);
    req.timeout = 30000;
    req.addEventListener("error", (evt) => {
      resolve({ hostname, error: "connection error" });
    });
    req.addEventListener("timeout", (evt) => {
      resolve({ hostname, error: "timeout" });
    });
    req.addEventListener("load", (evt) => {
      let securityInfo = evt.target.channel.securityInfo
                           .QueryInterface(Ci.nsITransportSecurityInfo);
      if (securityInfo.securityState &
          Ci.nsIWebProgressListener.STATE_CERT_USER_OVERRIDDEN) {
        resolve({ hostname, error: "user override" });
        return;
      }
      let sslStatus = securityInfo.QueryInterface(Ci.nsISSLStatusProvider)
                        .SSLStatus;
      let certdb = Cc["@mozilla.org/security/x509certdb;1"]
                     .getService(Ci.nsIX509CertDB);
      let result = new CertificateVerificationResult(hostname, resolve);
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

var sites = {
  "incoming.telemetry.mozilla.org": [
    "63eb34876cbd2ebbc3b254961d96cdafb00f28719229f61e27b19a2510929012",
    "154c433c491929c5ef686e838e323664a00e6a0d822ccc958fb4dab03e49a08f",
    "4348a0e9444c78cb265e058d5e8944b4d84f9662bd26db257f8934a443c70161"
  ],
  "telemetry.mozilla.org": [
    "197feaf3faa0f0ad637a89c97cb91336bfc114b6b3018203cbd9c3d10c7fa86c",
    "154c433c491929c5ef686e838e323664a00e6a0d822ccc958fb4dab03e49a08f",
    "4348a0e9444c78cb265e058d5e8944b4d84f9662bd26db257f8934a443c70161"
  ],
  "addons.mozilla.org": [
    "51646c662bb3fd3a3bac9d976803f4e6869183bb483b7d30dcdfc5c4d0487b41",
    "403e062a2653059113285baf80a0d4ae422c848c9f78fad01fc94bc5b87fef1a",
    "7431e5f4c3c1ce4690774f0b61e05440883ba9a01ed00ba6abd7806ed3b118cf"
  ],
  "services.addons.mozilla.org": [
    "51646c662bb3fd3a3bac9d976803f4e6869183bb483b7d30dcdfc5c4d0487b41",
    "403e062a2653059113285baf80a0d4ae422c848c9f78fad01fc94bc5b87fef1a",
    "7431e5f4c3c1ce4690774f0b61e05440883ba9a01ed00ba6abd7806ed3b118cf"
  ],
  "aus5.mozilla.org": [
    "60e8e2e092bdc3b69ce260d6a52f90fd6368768600f911a22ee9f1b8833abeea",
    "154c433c491929c5ef686e838e323664a00e6a0d822ccc958fb4dab03e49a08f",
    "4348a0e9444c78cb265e058d5e8944b4d84f9662bd26db257f8934a443c70161"
  ],
  "versioncheck.addons.mozilla.org": [
    "f7ac5873798f0322c206744901a8df1e944966be772e3a8bea2a4a9969fdfb38",
    "154c433c491929c5ef686e838e323664a00e6a0d822ccc958fb4dab03e49a08f",
    "4348a0e9444c78cb265e058d5e8944b4d84f9662bd26db257f8934a443c70161"
  ],
  "support.mozilla.org": [
    "1751e120f14ddbd5306d037aaa0dd753e2989cc4f6e5560b6821a6f807525147",
    "19400be5b7a31fb733917700789d2f0a2471c0c9d506c0e504c06c16d7cb17c0",
    "7431e5f4c3c1ce4690774f0b61e05440883ba9a01ed00ba6abd7806ed3b118cf"
  ],
  "ftp.mozilla.org": [
    "3b9ff6dc11f896b162603d29360be64e69f834e9b37a057a5b84cd54e58e7c8b",
    "154c433c491929c5ef686e838e323664a00e6a0d822ccc958fb4dab03e49a08f",
    "4348a0e9444c78cb265e058d5e8944b4d84f9662bd26db257f8934a443c70161"
  ],
  "mozilla.org": [
    "8a43602dc67d8c5934fa638c2b066d385918a1c3f5fd5307d13a7b363cd526d3",
    "403e062a2653059113285baf80a0d4ae422c848c9f78fad01fc94bc5b87fef1a",
    "7431e5f4c3c1ce4690774f0b61e05440883ba9a01ed00ba6abd7806ed3b118cf"
  ],
  "bugzilla.mozilla.org": [
    "1095a8c1e1c318fae495409911076de379abe5b02950ff40e8e863c4fdf39fcb",
    "403e062a2653059113285baf80a0d4ae422c848c9f78fad01fc94bc5b87fef1a",
    "7431e5f4c3c1ce4690774f0b61e05440883ba9a01ed00ba6abd7806ed3b118cf"
  ],
  "crash-reports.mozilla.com": [
    "58fe74d89c13624f79c9c97bcf9f2da14d22eb1e8d1caeeaee0735f8e68ef4a5",
    "403e062a2653059113285baf80a0d4ae422c848c9f78fad01fc94bc5b87fef1a",
    "7431e5f4c3c1ce4690774f0b61e05440883ba9a01ed00ba6abd7806ed3b118cf"
  ],
  "releases.mozilla.com": [
    "3b9ff6dc11f896b162603d29360be64e69f834e9b37a057a5b84cd54e58e7c8b",
    "154c433c491929c5ef686e838e323664a00e6a0d822ccc958fb4dab03e49a08f",
    "4348a0e9444c78cb265e058d5e8944b4d84f9662bd26db257f8934a443c70161"
  ],
  "download-installer.cdn.mozilla.net": [
    "6442cb8d30d303bc67c685ba319e9497aa39aeffc3caca9a707f151071ab3ca8",
    "154c433c491929c5ef686e838e323664a00e6a0d822ccc958fb4dab03e49a08f",
    "4348a0e9444c78cb265e058d5e8944b4d84f9662bd26db257f8934a443c70161"
  ],
  "firefox.settings.services.mozilla.com": [
    "ee6ddb1ac9614695a2c37579edb7844fa19fde18a490d1738e19cf0a49541918",
    "154c433c491929c5ef686e838e323664a00e6a0d822ccc958fb4dab03e49a08f",
    "4348a0e9444c78cb265e058d5e8944b4d84f9662bd26db257f8934a443c70161"
  ],
  "push.services.mozilla.com": [
    "ad3ef2e8244aa2d3575189a34311b274ceb8e9be323fe48c843e1f66bb62f6fe",
    "154c433c491929c5ef686e838e323664a00e6a0d822ccc958fb4dab03e49a08f",
    "4348a0e9444c78cb265e058d5e8944b4d84f9662bd26db257f8934a443c70161"
  ],
  "token.services.mozilla.com": [
    "dd123bd00f11e08d2995d907b80777edbff6169d2569d5d34f4fe10983d8901d",
    "154c433c491929c5ef686e838e323664a00e6a0d822ccc958fb4dab03e49a08f",
    "4348a0e9444c78cb265e058d5e8944b4d84f9662bd26db257f8934a443c70161"
  ],
  "shavar.services.mozilla.com": [
    "ab0cab1d1d1157eb5dff0ea41cd6d1eeebf59d1f123042954c61ea78003457d0",
    "154c433c491929c5ef686e838e323664a00e6a0d822ccc958fb4dab03e49a08f",
    "4348a0e9444c78cb265e058d5e8944b4d84f9662bd26db257f8934a443c70161"
  ],
  "search.services.mozilla.com": [
    "e5bd9cc4248f835d9e8d359bcac7b3e5073890b67b8e1e070a322e3e09ab0754",
    "154c433c491929c5ef686e838e323664a00e6a0d822ccc958fb4dab03e49a08f",
    "4348a0e9444c78cb265e058d5e8944b4d84f9662bd26db257f8934a443c70161"
  ]
};

function makeRequests() {
  let promises = [];
  for (let hostname of Object.keys(sites)) {
    promises.push(makeRequest(hostname));
  }
  return Promise.all(promises);
}

function analyzeAndReport(results) {
  let payload = { version: "1.0", mismatches: [] };
  for (let result of results) {
    // Skip if the connection resulted in any kind of error.
    if ("error" in result) {
      console.log(`${result.hostname}: ${result.error} - skipping`);
      continue;
    }
    // Skip imported roots.
    if (!result.chain[result.chain.length - 1].isBuiltInRoot) {
      console.log(`${result.hostname}: imported root - skipping`);
      continue;
    }

    let report = false;
    let expectedHashes = sites[result.hostname];
    // If we have chains of different length, obviously we'll have different
    // chains, so report this chain.
    if (expectedHashes.length != result.chain.length) {
      report = true;
    } else {
      // Otherwise, compare each hash. If we encounter an unexpected one, report
      // this chain.
      for (let i = 0; i < expectedHashes.length; i++) {
        let actualHash = result.chain[i].sha256Fingerprint.replace(/:/g, "")
                           .toLowerCase();
        if (actualHash != expectedHashes[i]) {
          report = true;
          break;
        }
      }
    }
    if (report) {
      payload.mismatches.push({ hostname: result.hostname,
                                chain: certArrayToBase64(result.chain) });
    }
  }
  console.log("deployment-checker results:");
  console.log(results);
  console.log("deployment-checker payload:");
  console.log(payload);
  return TelemetryController.submitExternalPing("deployment-checker", payload,
                                                {});
}

// We only run once - when installed.
function install() {
  // Only run if we have a good indication that we're not in a testing
  // environment (in which case attempting to connect to telemetry.mozilla.org
  // will result in a test failure).
  let telemetryServerURL = Preferences.get("toolkit.telemetry.server",
                                           undefined);
  // Also only run if the user has unified telemetry enabled (because we don't
  // want to submit a telemetry ping if they've opted out).
  let unifiedTelemetryEnabled = Preferences.get("toolkit.telemetry.unified",
                                                undefined);
  if (telemetryServerURL == "https://incoming.telemetry.mozilla.org" &&
      unifiedTelemetryEnabled === true) {
    makeRequests().then(analyzeAndReport).catch(Cu.reportError);
  }
}

function startup() {}
function shutdown() {}
function uninstall() {}
