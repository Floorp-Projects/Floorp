/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Some basic WebTransport tests for:
// * session rejection and redirection
// * session and stream creation
// * reading from incoming streams (uni & bidi)
// * writing to outgoing streams (uni & bidi)
//
// keep eslint happy until it knows about WebTransport
/* global WebTransport:false */

"use strict";

var h3Port;
var host;

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("network.dns.localDomains");
  Services.prefs.clearUserPref("network.webtransport.enabled");
  Services.prefs.clearUserPref("network.webtransport.datagrams.enabled");
  Services.prefs.clearUserPref("network.webtransport.redirect.enabled");
});

var { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

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

add_task(async function setup() {
  Services.prefs.setCharPref("network.dns.localDomains", "foo.example.com");
  Services.prefs.setBoolPref("network.webtransport.enabled", true);
  Services.prefs.setBoolPref("network.webtransport.datagrams.enabled", true);
  Services.prefs.setBoolPref("network.webtransport.redirect.enabled", true);

  h3Port = Services.env.get("MOZHTTP3_PORT");
  Assert.notEqual(h3Port, null);
  Assert.notEqual(h3Port, "");
  host = "foo.example.com:" + h3Port;
  do_get_profile();

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  // `../unit/` so that unit_ipc tests can use as well
  addCertFromFile(
    certdb,
    "../../../../netwerk/test/unit/http2-ca.pem",
    "CTu,u,u"
  );
});

add_task(async function test_webtransport_create() {
  const wt = new WebTransport("https://" + host + "/success");
  registerCleanupFunction(async () => {
    wt.close();
  });
  await wt.ready;
  wt.close();
});

add_task(async function test_redirect_wt() {
  let wt = new WebTransport("https://" + host + "/redirect");
  registerCleanupFunction(async () => {
    wt.close();
  });
  const e1 = await wt.ready.catch(e => e);
  const e2 = await wt.closed.catch(e => e);

  info("redirect ready error: " + e1);
  Assert.equal(e1, "WebTransportError: WebTransport connection rejected");
  Assert.equal(e2, "WebTransportError: WebTransport connection rejected");
});

add_task(async function test_reject_wt() {
  let wt = new WebTransport("https://" + host + "/reject");
  registerCleanupFunction(async () => {
    wt.close();
  });
  const e1 = await wt.ready.catch(e => e);
  const e2 = await wt.closed.catch(e => e);
  Assert.equal(e1, "WebTransportError: WebTransport connection rejected");
  Assert.equal(e2, "WebTransportError: WebTransport connection rejected");
});

add_task(async function test_immediate_server_close() {
  let wt = new WebTransport("https://" + host + "/closeafter0ms");
  registerCleanupFunction(async () => {
    wt.close();
  });
  await wt.ready;
  await wt.closed;
  Assert.ok(true);
});

add_task(async function test_delayed_server_close() {
  let wt = new WebTransport("https://" + host + "/closeafter100ms");
  registerCleanupFunction(async () => {
    wt.close();
  });
  await wt.ready;
  Assert.ok(true);
});

add_task(async function test_wt_stream_create() {
  let wt = new WebTransport("https://" + host + "/success");
  registerCleanupFunction(async () => {
    wt.close();
  });
  await wt.ready;

  let bds = await wt.createBidirectionalStream();
  Assert.notEqual(bds, null);
  let uds = await wt.createUnidirectionalStream();
  Assert.notEqual(uds, null);
  wt.close();
});

async function readOnceBi(wt) {
  // TODO: probably a cleaner way to do this
  let str = "";
  for await (const { readable } of wt.incomingBidirectionalStreams) {
    for await (const buffer of readable) {
      for (const c of buffer) {
        str += String.fromCharCode(c);
      }
      break;
    }
    break;
  }
  return str;
}

async function readDataInner(receiveStream) {
  // inner function necessary for some reason
  const reader = receiveStream.getReader();
  const read = await reader.read();
  let str = "";
  for (const c of read.value) {
    str += String.fromCharCode(c);
  }
  return str;
}

async function readDataUni(uds) {
  const reader = uds.getReader();
  const read = await reader.read();
  return readDataInner(read.value);
}

add_task(async function test_wt_incoming_unidi_stream() {
  let wt = new WebTransport("https://" + host + "/create_unidi_stream");
  registerCleanupFunction(async () => {
    wt.close();
  });
  await wt.ready;

  let str = await readDataUni(wt.incomingUnidirectionalStreams);
  Assert.equal(str, "0123456789");
  wt.close();
});

add_task(async function test_wt_outgoing_unidi_stream() {
  let wt = new WebTransport("https://" + host + "/create_unidi_stream");
  registerCleanupFunction(async () => {
    wt.close();
  });
  await wt.ready;

  let expected = "uni_hello";
  let writableStream = await wt.createUnidirectionalStream();
  let wsDefaultWriter = writableStream.getWriter();
  await wsDefaultWriter.ready; // sanity check
  let data = new TextEncoder().encode(expected);
  await wsDefaultWriter.write(data);

  // TODO: fix readDataUni causes `error is InvalidStreamId` (still works though)
  let v1 = await readDataUni(wt.incomingUnidirectionalStreams);
  Assert.equal(v1, "0123456789");
  wt.close();

  // TODO: improve this clunky way of checking that the server received
  let wt2 = new WebTransport("https://" + host + "/create_unidi_stream");
  registerCleanupFunction(async () => {
    wt2.close();
  });
  await wt2.ready;

  let v2 = await readDataUni(wt2.incomingUnidirectionalStreams);
  Assert.equal(v2, expected);
  wt2.close();
});

add_task(async function test_wt_incoming_bidi_stream() {
  let wt = new WebTransport("https://" + host + "/create_bidi_stream");
  registerCleanupFunction(async () => {
    wt.close();
  });
  await wt.ready;

  const str = await readOnceBi(wt);
  Assert.equal(str, "abcdefghij");
  wt.close();
});

add_task(async function test_wt_outgoing_bidi_stream() {
  let wt = new WebTransport("https://" + host + "/success");
  registerCleanupFunction(async () => {
    wt.close();
  });
  await wt.ready;

  // write to server
  let wtbds = await wt.createBidirectionalStream();
  let writableStream = wtbds.writable;
  let wsDefaultWriter = writableStream.getWriter();
  let expected = "xyzhello";
  let data = new TextEncoder().encode(expected);
  await wsDefaultWriter.write(data);

  // string goes through server and is echo'd back here
  let rs = wtbds.readable;
  let readstr = await readDataInner(rs);
  Assert.equal(readstr, expected);
  wt.close();
});

// TODO: datagram test
// TODO: getStats tests
