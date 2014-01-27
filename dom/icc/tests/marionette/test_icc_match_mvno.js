/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "icc_header.js";

// Emulator's hard coded IMSI: 310260000000000
let testCases = [
  // mvno type, mvno data, request success, expected result
  ["imsi", "3102600",            true, true               ],
  // x and X means skip the comparison.
  ["imsi", "31026xx0",           true, true               ],
  ["imsi", "310260x0x",          true, true               ],
  ["imsi", "310260X00",          true, true               ],
  ["imsi", "310260XX1",          true, false              ],
  ["imsi", "31026012",           true, false              ],
  ["imsi", "310260000000000",    true, true               ],
  ["imsi", "310260000000000123", true, false              ],
  ["imsi", "",                   false, "InvalidParameter"],
  // Currently we only support imsi match.
  ["spn",  "Android",            false, "ModeNotSupported"]
];

function matchMvno(mvnoType, mvnoData, success, expectedResult) {
  log("matchMvno: " + mvnoType + ", " + mvnoData);
  let request = icc.matchMvno(mvnoType, mvnoData);
  request.onsuccess = function onsuccess() {
    log("onsuccess: " + request.result);
    ok(success, "onsuccess while error expected");
    is(request.result, expectedResult);
    testMatchMvno();
  }
  request.onerror = function onerror() {
    log("onerror: " + request.error.name);
    ok(!success, "onerror while success expected");
    is(request.error.name, expectedResult);
    testMatchMvno();
  }
}

function testMatchMvno() {
  let testCase = testCases.shift();
  if (!testCase) {
    taskHelper.runNext();
    return;
  }
  matchMvno(testCase[0], testCase[1], testCase[2], testCase[3]);
}

taskHelper.push(
  testMatchMvno
);

// Start test
taskHelper.runNext();

