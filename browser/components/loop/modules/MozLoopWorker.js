/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A worker dedicated to loop-report sanitation and writing for MozLoopService.
 */

"use strict";

importScripts("resource://gre/modules/osfile.jsm");

let File = OS.File;
let Encoder = new TextEncoder();
let Counter = 0;

const MAX_LOOP_LOGS = 5;
/**
 * Communications with the controller.
 *
 * Accepts messages:
 * { path: filepath, ping: data }
 *
 * Sends messages:
 * { ok: true }
 * { fail: serialized_form_of_OS.File.Error }
 */

onmessage = function(e) {
  if (++Counter > MAX_LOOP_LOGS) {
    postMessage({
      fail: "Maximum " + MAX_LOOP_LOGS + "loop reports reached for this session"
    });
    return;
  }

  let directory = e.data.directory;
  let filename = e.data.filename;
  let ping = e.data.ping;

  // Anonymize data
  resetIpMask();
  ping.payload.localSdp = redactSdp(ping.payload.localSdp);
  ping.payload.remoteSdp = redactSdp(ping.payload.remoteSdp);
  ping.payload.log = sanitizeLogs(ping.payload.log);

  let pingStr = anonymizeIPv4(sanitizeUrls(JSON.stringify(ping)));

  // Save to disk
  let array = Encoder.encode(pingStr);
  try {
    File.makeDir(directory,
                 { unixMode: OS.Constants.S_IRWXU, ignoreExisting: true });
    File.writeAtomic(OS.Path.join(directory, filename), array);
    postMessage({ ok: true });
  } catch (ex if ex instanceof File.Error) {
    // Instances of OS.File.Error know how to serialize themselves
    postMessage({fail: File.Error.toMsg(ex)});
  }
};

/**
 * Mask upper 24-bits of ip address with fake numbers. Call resetIpMask() first.
 */

let IpMap = {};
let IpCount = 0;

function resetIpMask() {
  IpMap = {};
  IpCount = Math.floor(Math.random() * 16777215) + 1;
}

/**
 * Masks upper 24-bits of ip address with fake numbers. Grunt function.
 *
 * @param {DOMString} ip address
 */
function maskIp(ip) {
  let isInvalidOrRfc1918or3927 = function(p1, p2, p3, p4) {
    let invalid = octet => octet < 0 || octet > 255;
    return invalid(p1) || invalid(p2) || invalid(p3) || invalid(p4) ||
    (p1 == 10) ||
    (p1 == 172 && p2 >= 16 && p2 <= 31) ||
    (p1 == 192 && p2 == 168) ||
    (p1 == 169 && p2 == 254);
  };

  let [p1, p2, p3, p4] = ip.split(".");

  if (isInvalidOrRfc1918or3927(p1, p2, p3, p4)) {
    return ip;
  }
  let key = [p1, p2, p3].join();
  if (!IpMap[key]) {
    do {
      IpCount = (IpCount + 1049039) % 16777216; // + prime % 2^24
      p1 = (IpCount >> 16) % 256;
      p2 = (IpCount >> 8) % 256;
      p3 = IpCount % 256;
    } while (isInvalidOrRfc1918or3927(p1, p2, p3, p4));
    IpMap[key] = p1 + "." + p2 + "." + p3;
  }
  return IpMap[key] + "." + p4;
}

/**
 * Partially masks ip numbers in input text.
 *
 * @param {DOMString} text Input text containing IP numbers as words.
 */
function anonymizeIPv4(text) {
  return text.replace(/\b\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\b/g,
                      maskIp.bind(this));
}

/**
 * Sanitizes any urls of session information, like
 *
 *   - (id=31 url=https://call.services.mozilla.com/#call/ongoing/AQHYjqH_...)
 *   + (id=31 url=https://call.services.mozilla.com/#call/xxxx)
 *
 *   - (id=35 url=about:loopconversation#incoming/1403134352854)
 *   + (id=35 url=about:loopconversation#incoming/xxxx)
 *
 * @param {DOMString} text The text.
 */
function sanitizeUrls(text) {
  let trimUrl = url => url.replace(/(#call|#incoming).*/g,
                                   (match, type) => type + "/xxxx");
  return text.replace(/\(id=(\d+) url=([^\)]+)\)/g,
                      (match, id, url) =>
                      "(id=" + id + " url=" + trimUrl(url) + ")");
}

/**
 * Removes privacy sensitive information from SDP input text outright, like
 *
 *   a=fingerprint:sha-256 E9:DE:6A:FE:2A:2F:05: etc.
 *   a=identity ...
 *
 * Redacts lines from match to EOL. Assumes \r\n\ linebreaks.
 *
 * @param {DOMString} sdp The sdp text.
 */
let redactSdp = sdp => sdp.replace(/\r\na=(fingerprint|identity):.*?\r\n/g,
                                   "\r\n");

/**
 * Sanitizes log text of sensitive information, like
 *
 *   - srflx(IP4:192.168.1.3:60348/UDP|turn402-oak.tokbox.com:3478)
 *   + srflx(IP4:192.168.1.3:60348/UDP|xxxx.xxx)
 *
 * @param {DOMString} log The log text.
 */
function sanitizeLogs(log) {
  let rex = /(srflx|relay)\(IP4:\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}:\d{1,5}\/(UDP|TCP)\|[^\)]+\)/g;

  return log.replace(rex, match => match.replace(/\|[^\)]+\)/, "|xxxx.xxx)"));
}
