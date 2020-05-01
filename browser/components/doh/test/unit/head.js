/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);

const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

let h2Port, trrServer1, trrServer2;
const { DNSLookup, LookupAggregator, TRRRacer } = ChromeUtils.import(
  "resource:///modules/TRRPerformance.jsm"
);

function readFile(file) {
  let fstream = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  fstream.init(file, -1, 0, 0);
  let data = NetUtil.readInputStreamToString(fstream, fstream.available());
  fstream.close();
  return data;
}

function addCertFromFile(certdb, filename, trustString) {
  let certFile = do_get_file(filename, false);
  let pem = readFile(certFile)
    .replace(/-----BEGIN CERTIFICATE-----/, "")
    .replace(/-----END CERTIFICATE-----/, "")
    .replace(/[\r\n]/g, "");
  certdb.addCertFromBase64(pem, trustString);
}

function ensureNoTelemetry() {
  let events =
    Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      true
    ).parent || [];
  events = events.filter(e => e[1] == "security.doh.trrPerformance");
  Assert.ok(!events.length);
}

function setup() {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  h2Port = env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  // Set to allow the cert presented by our H2 server
  do_get_profile();

  Services.prefs.setBoolPref("network.http.spdy.enabled", true);
  Services.prefs.setBoolPref("network.http.spdy.enabled.http2", true);

  // use the h2 server as DOH provider
  trrServer1 = `https://foo.example.com:${h2Port}/doh?responseIP=1.1.1.1`;
  trrServer2 = `https://foo.example.com:${h2Port}/doh?responseIP=2.2.2.2`;
  // make all native resolve calls "secretly" resolve localhost instead
  Services.prefs.setBoolPref("network.dns.native-is-localhost", true);

  // The moz-http2 cert is for foo.example.com and is signed by http2-ca.pem
  // so add that cert to the trust list as a signing cert.  // the foo.example.com domain name.
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  Services.prefs.setCharPref(
    "network.trr.resolvers",
    `[{"url": "${trrServer1}"}, {"url": "${trrServer2}"}]`
  );

  Services.prefs.setIntPref("doh-rollout.trrRace.randomSubdomainCount", 2);

  Services.prefs.setCharPref(
    "doh-rollout.trrRace.popularDomains",
    "foo.example.com, bar.example.com"
  );

  Services.prefs.setCharPref(
    "doh-rollout.trrRace.canonicalDomain",
    "firefox-dns-perf-test.net"
  );

  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("network.http.spdy.enabled");
    Services.prefs.clearUserPref("network.http.spdy.enabled.http2");
    Services.prefs.clearUserPref("network.dns.native-is-localhost");
    Services.prefs.clearUserPref("network.trr.resolvers");

    Services.telemetry.canRecordExtended = oldCanRecord;
  });
}
