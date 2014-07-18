/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 30000;
MARIONETTE_HEAD_JS = "icc_header.js";

let testCases = [
  // mvno type, mvno data, request success, expected result
  // Emulator's hard coded IMSI: 310260000000000
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
  // Emulator's hard coded SPN:  Android
  ["spn",  "Android",            true, true               ],
  ["spn",  "",                   false, "InvalidParameter"],
  ["spn",  "OneTwoThree",        true, false              ],
  // Emulator's hard coded GID1: 5a4d
  ["gid",  "",                   false, "InvalidParameter"],
  ["gid",  "A1",                 true, false              ],
  ["gid",  "5A",                 true, true               ],
  ["gid",  "5a",                 true, true               ],
  ["gid",  "5a4d",               true, true               ],
  ["gid",  "5A4D",               true, true               ],
  ["gid",  "5a4d6c",             true, false              ]
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

