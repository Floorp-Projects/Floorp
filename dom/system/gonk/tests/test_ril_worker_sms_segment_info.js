/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

const ESCAPE = "\uffff";
const RESCTL = "\ufffe";
const SP = " ";

function run_test() {
  run_next_test();
}

/**
 * Verify RadioInterface#_countGsm7BitSeptets() and
 * GsmPDUHelper#writeStringAsSeptets() algorithm match each other.
 */
add_test(function test_RadioInterface__countGsm7BitSeptets() {
  let ril = newRadioInterface();

  let worker = newWorker({
    postRILMessage: function fakePostRILMessage(data) {
      // Do nothing
    },
    postMessage: function fakePostMessage(message) {
      // Do nothing
    }
  });

  let helper = worker.GsmPDUHelper;
  helper.resetOctetWritten = function() {
    helper.octetsWritten = 0;
  };
  helper.writeHexOctet = function() {
    helper.octetsWritten++;
  };

  function do_check_calc(str, expectedCalcLen, lst, sst, strict7BitEncoding, strToWrite) {
    do_check_eq(expectedCalcLen,
                ril._countGsm7BitSeptets(str,
                                         PDU_NL_LOCKING_SHIFT_TABLES[lst],
                                         PDU_NL_SINGLE_SHIFT_TABLES[sst],
                                         strict7BitEncoding));

    helper.resetOctetWritten();
    strToWrite = strToWrite || str;
    helper.writeStringAsSeptets(strToWrite, 0, lst, sst);
    do_check_eq(Math.ceil(expectedCalcLen * 7 / 8), helper.octetsWritten);
  }

  // Test calculation encoded message length using both locking/single shift tables.
  for (let lst = 0; lst < PDU_NL_LOCKING_SHIFT_TABLES.length; lst++) {
    let langTable = PDU_NL_LOCKING_SHIFT_TABLES[lst];

    let str = langTable.substring(0, PDU_NL_EXTENDED_ESCAPE)
              + langTable.substring(PDU_NL_EXTENDED_ESCAPE + 1);

    for (let sst = 0; sst < PDU_NL_SINGLE_SHIFT_TABLES.length; sst++) {
      let langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[sst];

      // <escape>, <resctrl> should be ignored.
      do_check_calc(ESCAPE + RESCTL, 0, lst, sst);

      // Characters defined in locking shift table should be encoded directly.
      do_check_calc(str, str.length, lst, sst);

      let [str1, str2] = ["", ""];
      for (let i = 0; i < langShiftTable.length; i++) {
        if ((i == PDU_NL_EXTENDED_ESCAPE) || (i == PDU_NL_RESERVED_CONTROL)) {
          continue;
        }

        let c = langShiftTable[i];
        if (langTable.indexOf(c) >= 0) {
          str1 += c;
        } else {
          str2 += c;
        }
      }

      // Characters found in both locking/single shift tables should be
      // directly encoded.
      do_check_calc(str1, str1.length, lst, sst);

      // Characters found only in single shift tables should be encoded as
      // <escape><code>, therefore doubles its original length.
      do_check_calc(str2, str2.length * 2, lst, sst);
    }
  }

  // Bug 790192: support strict GSM SMS 7-Bit encoding
  let str = "", strToWrite = "", gsmLen = 0;
  for (let c in GSM_SMS_STRICT_7BIT_CHARMAP) {
    str += c;
    strToWrite += GSM_SMS_STRICT_7BIT_CHARMAP[c];
    if (PDU_NL_LOCKING_SHIFT_TABLES.indexOf(GSM_SMS_STRICT_7BIT_CHARMAP[c])) {
      gsmLen += 1;
    } else {
      gsmLen += 2;
    }
  }
  do_check_calc(str, gsmLen,
                PDU_NL_IDENTIFIER_DEFAULT, PDU_NL_IDENTIFIER_DEFAULT,
                true, strToWrite);

  run_next_test();
});

/**
 * Verify RadioInterface#calculateUserDataLength handles national language
 * selection correctly.
 */
add_test(function test_RadioInterface__calculateUserDataLength() {
  let ril = newRadioInterface();

  function test_calc(str, expected, enabledGsmTableTuples, strict7BitEncoding) {
    ril.enabledGsmTableTuples = enabledGsmTableTuples;
    let options = ril._calculateUserDataLength(str, strict7BitEncoding);

    do_check_eq(expected[0], options.dcs);
    do_check_eq(expected[1], options.encodedFullBodyLength);
    do_check_eq(expected[2], options.userDataHeaderLength);
    do_check_eq(expected[3], options.langIndex);
    do_check_eq(expected[4], options.langShiftIndex);
  }

  // Test UCS fallback
  // - No any default enabled nl tables
  test_calc("A", [PDU_DCS_MSG_CODING_16BITS_ALPHABET, 2, 0,], []);
  // - Character not defined in enabled nl tables
  test_calc("A", [PDU_DCS_MSG_CODING_16BITS_ALPHABET, 2, 0,], [[2, 2]]);

  // With GSM default nl tables
  test_calc("A", [PDU_DCS_MSG_CODING_7BITS_ALPHABET, 1, 0, 0, 0], [[0, 0]]);
  // - SP is defined in both locking/single shift tables, should be directly
  //   encoded.
  test_calc(SP, [PDU_DCS_MSG_CODING_7BITS_ALPHABET, 1, 0, 0, 0], [[0, 0]]);
  // - '^' is only defined in single shift table, should be encoded as
  //   <escape>^.
  test_calc("^", [PDU_DCS_MSG_CODING_7BITS_ALPHABET, 2, 0, 0, 0], [[0, 0]]);

  // Test userDataHeaderLength calculation
  // - Header contains both IEIs
  test_calc("A", [PDU_DCS_MSG_CODING_7BITS_ALPHABET, 1, 6, 1, 1], [[1, 1]]);
  // - Header contains only locking shift table IEI
  test_calc("A", [PDU_DCS_MSG_CODING_7BITS_ALPHABET, 1, 3, 1, 0], [[1, 0]]);
  // - Header contains only single shift table IEI
  test_calc("^", [PDU_DCS_MSG_CODING_7BITS_ALPHABET, 2, 3, 0, 1], [[0, 1]]);

  // Test minimum cost nl tables selection
  // - 'A' is defined in locking shift table
  test_calc("A", [PDU_DCS_MSG_CODING_7BITS_ALPHABET, 1, 3, 1, 0], [[1, 0], [2, 0]]);
  test_calc("A", [PDU_DCS_MSG_CODING_7BITS_ALPHABET, 1, 3, 1, 0], [[2, 0], [1, 0]]);
  // - 'A' is defined in single shift table
  test_calc("A", [PDU_DCS_MSG_CODING_7BITS_ALPHABET, 2, 6, 2, 4], [[2, 0], [2, 4]]);
  test_calc("A", [PDU_DCS_MSG_CODING_7BITS_ALPHABET, 2, 6, 2, 4], [[2, 4], [2, 0]]);
  // - 'A' is defined in locking shift table of one tuple and in single shift
  //   table of another.
  test_calc("A", [PDU_DCS_MSG_CODING_7BITS_ALPHABET, 1, 3, 1, 0], [[1, 0], [2, 4]]);
  test_calc("A", [PDU_DCS_MSG_CODING_7BITS_ALPHABET, 1, 3, 1, 0], [[2, 4], [1, 0]]);

  // Test Bug 733981
  // - Case 1, headerLen is in octets, not septets. "\\" is defined in default
  //   single shift table and Portuguese locking shift table. The original code
  //   will add headerLen 7(octets), which should be 8(septets), to calculated
  //   cost and gets 14, which should be 15 in total for the first run. As for
  //   the second run, it will be undoubtedly 14 in total. With correct fix,
  //   the best choice should be the second one.
  test_calc("\\\\\\\\\\\\\\",
            [PDU_DCS_MSG_CODING_7BITS_ALPHABET, 14, 0, 0, 0], [[3, 1], [0, 0]]);
  // - Case 2, possible early return non-best choice. The original code will
  //   get total cost 6 in the first run and returns immediately. With correct
  //   fix, the best choice should be the second one.
  test_calc(ESCAPE + ESCAPE + ESCAPE + ESCAPE + ESCAPE + "\\",
            [PDU_DCS_MSG_CODING_7BITS_ALPHABET, 2, 0, 0, 0], [[3, 0], [0, 0]]);

  // Test Bug 790192: support strict GSM SMS 7-Bit encoding
  let str = "", gsmLen = 0, udhl = 0;
  for (let c in GSM_SMS_STRICT_7BIT_CHARMAP) {
    str += c;
    if (PDU_NL_LOCKING_SHIFT_TABLES.indexOf(GSM_SMS_STRICT_7BIT_CHARMAP[c])) {
      gsmLen += 1;
    } else {
      gsmLen += 2;
    }
  }
  if (str.length > PDU_MAX_USER_DATA_UCS2) {
    udhl = 5;
  }
  test_calc(str, [PDU_DCS_MSG_CODING_7BITS_ALPHABET, gsmLen, 0, 0, 0], [[0, 0]], true);
  test_calc(str, [PDU_DCS_MSG_CODING_16BITS_ALPHABET, str.length * 2, udhl], [[0, 0]]);

  run_next_test();
});

function generateStringOfLength(str, length) {
  while (str.length < length) {
    if (str.length < (length / 2)) {
      str = str + str;
    } else {
      str = str + str.substr(0, length - str.length);
    }
  }

  return str;
}

/**
 * Verify RadioInterface#_calculateUserDataLength7Bit multipart handling.
 */
add_test(function test_RadioInterface__calculateUserDataLength7Bit_multipart() {
  let ril = newRadioInterface();

  function test_calc(str, expected) {
    let options = ril._calculateUserDataLength7Bit(str);

    do_check_eq(expected[0], options.encodedFullBodyLength);
    do_check_eq(expected[1], options.userDataHeaderLength);
    do_check_eq(expected[2], options.segmentMaxSeq);
  }

  test_calc(generateStringOfLength("A", PDU_MAX_USER_DATA_7BIT),
            [PDU_MAX_USER_DATA_7BIT, 0, 1]);
  test_calc(generateStringOfLength("A", PDU_MAX_USER_DATA_7BIT + 1),
            [PDU_MAX_USER_DATA_7BIT + 1, 5, 2]);

  run_next_test();
});

/**
 * Verify RadioInterface#_fragmentText().
 */
add_test(function test_RadioInterface__fragmentText7Bit() {
  let ril = newRadioInterface();

  function test_calc(str, strict7BitEncoding, expectedSegments) {
    expectedSegments = expectedSegments || 1;
    let options = ril._fragmentText(str, null, strict7BitEncoding);
    do_check_eq(expectedSegments, options.segments.length);
  }

  // 7-Bit

  // Boundary checks
  test_calc(generateStringOfLength("A", PDU_MAX_USER_DATA_7BIT), false);
  test_calc(generateStringOfLength("A", PDU_MAX_USER_DATA_7BIT), true);
  test_calc(generateStringOfLength("A", PDU_MAX_USER_DATA_7BIT + 1), false, 2);
  test_calc(generateStringOfLength("A", PDU_MAX_USER_DATA_7BIT + 1), true, 2);

  // Escaped character
  test_calc(generateStringOfLength("{", PDU_MAX_USER_DATA_7BIT / 2), false);
  test_calc(generateStringOfLength("{", PDU_MAX_USER_DATA_7BIT / 2 + 1), false, 2);
  // Escaped character cannot be separated
  test_calc(generateStringOfLength("{", (PDU_MAX_USER_DATA_7BIT - 7) * 2 / 2), false, 3);

  // Test headerLen, 7 = Math.ceil(6 * 8 / 7), 6 = headerLen + 1
  test_calc(generateStringOfLength("A", PDU_MAX_USER_DATA_7BIT - 7));
  test_calc(generateStringOfLength("A", (PDU_MAX_USER_DATA_7BIT - 7) * 2), false, 2);
  test_calc(generateStringOfLength("A", (PDU_MAX_USER_DATA_7BIT - 7) * 3), false, 3);

  // UCS-2

  // Boundary checks
  test_calc(generateStringOfLength("\ua2db", PDU_MAX_USER_DATA_UCS2));
  test_calc(generateStringOfLength("\ua2db", PDU_MAX_USER_DATA_UCS2), true);
  test_calc(generateStringOfLength("\ua2db", PDU_MAX_USER_DATA_UCS2 + 1), false, 2);
  // Bug 816082: when strict GSM SMS 7-Bit encoding is enabled, replace unicode
  // chars with '*'.
  test_calc(generateStringOfLength("\ua2db", PDU_MAX_USER_DATA_UCS2 + 1), true, 1);

  // UCS2 character cannot be separated
  ril.segmentRef16Bit = true;
  test_calc(generateStringOfLength("\ua2db", (PDU_MAX_USER_DATA_UCS2 * 2 - 7) * 2 / 2), false, 3);
  ril.segmentRef16Bit = false;

  // Test Bug 790192: support strict GSM SMS 7-Bit encoding
  for (let c in GSM_SMS_STRICT_7BIT_CHARMAP) {
    test_calc(generateStringOfLength(c, PDU_MAX_USER_DATA_7BIT), false, 3);
    test_calc(generateStringOfLength(c, PDU_MAX_USER_DATA_7BIT), true);
    test_calc(generateStringOfLength(c, PDU_MAX_USER_DATA_UCS2), false);
  }

  run_next_test();
});
