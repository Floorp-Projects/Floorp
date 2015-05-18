/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* globals run_next_test, add_test, ok, Components, SEUtils */
/* exported run_test */

Components.utils.import("resource://gre/modules/SEUtils.jsm");
let GP = {};
Components.utils.import("resource://gre/modules/gp_consts.js", GP);

const VALID_HEX_STR = "0123456789ABCDEF";
const VALID_BYTE_ARR = [0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF];

// This set should be what the actual ACE uses.
let containerTags = [
  GP.TAG_SEQUENCE,
  GP.TAG_FCP,
  GP.TAG_GPD_AID,
  GP.TAG_EXTERNALDO,
  GP.TAG_INDIRECT,
  GP.TAG_EF_ODF
];

function run_test() {
  ok(!!SEUtils, "SEUtils should be available");
  run_next_test();
}

add_test(function test_byteArrayToHexString() {
  let hexStr = SEUtils.byteArrayToHexString(VALID_BYTE_ARR);
  ok(hexStr === VALID_HEX_STR,
     "should convert byte Array to uppercased hex string");

  [[], null, undefined].forEach((input) => {
    hexStr = SEUtils.byteArrayToHexString(input);
    ok(hexStr === "", "invalid arg:" + input + " should return empty string");
  });

  run_next_test();
});

add_test(function test_hexStringToByteArray() {
  let byteArr = SEUtils.hexStringToByteArray(VALID_HEX_STR);
  ok(SEUtils.arraysEqual(byteArr, VALID_BYTE_ARR),
     "should convert uppercased string to byte Array");

  byteArr = SEUtils.hexStringToByteArray(VALID_HEX_STR.toLowerCase());
  ok(SEUtils.arraysEqual(byteArr, VALID_BYTE_ARR),
     "should convert lowercased string to byte Array");

  ["", null, undefined, "123"].forEach((input) => {
    byteArr = SEUtils.hexStringToByteArray(input);
    ok(Array.isArray(byteArr) && byteArr.length === 0,
       "invalid arg: " + input + " should be empty Array");
  });

  run_next_test();
});

add_test(function test_arraysEqual() {
  ok(SEUtils.arraysEqual([1, 2, 3], [1, 2, 3]),
     "should return true on equal Arrays");

  [[1], [1, 2, 4], [3, 2, 1]].forEach((input) => {
    ok(!SEUtils.arraysEqual([1, 2, 3], input),
       "should return false when Arrays not equal");
  });

  [null, undefined].forEach((input) => {
    ok(!SEUtils.arraysEqual([1, 2, 3], input),
       "should return false when comparing Array with invalid argument");

    ok(!SEUtils.arraysEqual(input, input),
       "should return false when both args are invalid");
  });

  run_next_test();
});

add_test(function test_ensureIsArray() {
  let obj = {a: "a"};
  let targetArray = [obj];
  let result = null;

  result = SEUtils.ensureIsArray(obj);
  ok(targetArray[0].a === result[0].a,
     "should return true if array element contains the same value");
  deepEqual(result, targetArray,
            "result should be deeply equal to targetArray");

  result = SEUtils.ensureIsArray(targetArray);
  deepEqual(result, targetArray,
            "ensureIsAray with an array should return same array value.");

  run_next_test();
});

add_test(function test_parseTLV_empty() {
  let containerTags = [];
  let result = null;

  // Base:
  result = SEUtils.parseTLV([], []);
  deepEqual({}, result,
     "empty parse input should result in an " +
     "empty object (internal SEUtils format only).");
  run_next_test();
});

add_test(function test_parseTLV_selectResponse() {
  let result = null;
  let hexStr = "62 27 82 02 78 21 83 02 7F 50 A5 06 83 04 00 04 C1 DC 8A" +
               "01 05 8B 06 2F 06 01 16 00 14 C6 06 90 01 00 83 01 01 81" +
               "02 FF FF";

  let expected = {
    0x62: {
      0x82: [0x78, 0x21],
      0x83: [0x7F, 0x50],
      0xA5: {
        0x83: [0x00, 0x04, 0xC1, 0xDC]
      },
      0x8A: [0x05],
      0x8B: [0x2F, 0x06, 0x01, 0x16, 0x00, 0x14],
      0xC6: [0x90, 0x01, 0x00, 0x83, 0x01, 0x01],
      0x81: [0xFF, 0xFF]
    }
  };

  result = SEUtils.parseTLV(formatHexAndCreateByteArray(hexStr), containerTags);
  deepEqual(result, expected,
            "parsed real selectResponse should equal the expected rules");
  run_next_test();
});

add_test(function test_parseTLV_DODF() {
  let result = null;
  let hexStr = "A1 29 30 00 30 0F 0C 0D 47 50 20 53 45 20 41 63 63 20 43" +
               "74 6C A1 14 30 12 06 0A 2A 86 48 86 FC 6B 81 48 01 01 30" +
               "04 04 02 43 00 A1 2B 30 00 30 0F 0C 0D 53 41 54 53 41 20" +
               "47 54 4F 20 31 2E 31 A1 16 30 14 06 0C 2B 06 01 04 01 2A" +
               "02 6E 03 01 01 01 30 04 04 02 45 31 FF FF FF FF FF FF FF" +
               "FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF" +
               "FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF" +
               "FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF" +
               "FF FF FF FF FF FF FF FF FF FF FF FF";

  let expected = {
    0xA1: [
      {
        0x30: [
          {},
          {
            0x0C: [0x47, 0x50, 0x20, 0x53, 0x45, 0x20, 0x41, 0x63, 0x63,
                   0x20, 0x43, 0x74, 0x6C]
          }
        ],
        0xA1: {
          0x30: {
            0x06: [0x2A, 0x86, 0x48, 0x86, 0xFC, 0x6B, 0x81, 0x48, 0x01,
                   0x01],
            0x30: {
              0x04: [0x43, 0x00]
            }
          }
        }
      },
      {
        0x30: [
          {},
          {
            0x0C: [0x53, 0x41, 0x54, 0x53, 0x41, 0x20, 0x47, 0x54, 0x4F,
                   0x20, 0x31, 0x2E, 0x31]
          }
        ],
        0xA1: {
          0x30: {
            0x06: [0x2B, 0x06, 0x01, 0x04, 0x01, 0x2A, 0x02, 0x6E, 0x03,
                   0x01, 0x01, 0x01],
            0x30: {
              0x04: [0x45, 0x31]
            }
          }
        }
      }
    ]
  };

  result = SEUtils.parseTLV(formatHexAndCreateByteArray(hexStr), containerTags);
  deepEqual(result, expected,
            "Real Access Control Enforcer DODF file, with 0xFF padding. " +
            "Should equal expected rules.");
  run_next_test();
});

add_test(function test_parseTLV_acRules() {
  let result = null;
  let hexStr = "30 08 82 00 30 04 04 02 43 11 FF FF FF FF FF FF FF FF FF" +
               "FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF" +
               "FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF" +
               "FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF" +
               "FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF" +
               "FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF" +
               "FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF" +
               "FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF" +
               "FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF" +
               "FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF" +
               "FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF" +
               "FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF" +
               "FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF FF" +
               "FF FF FF FF FF FF FF FF FF";

  let expected = {
    0x30: {
      0x82: [],
      0x30: {
        0x04: [0x43, 0x11]
      }
    }
  };

  result = SEUtils.parseTLV(formatHexAndCreateByteArray(hexStr), containerTags);
  deepEqual(result, expected,
            "Parsed Access Control Rules should equal the expected rules");
  run_next_test();
});
