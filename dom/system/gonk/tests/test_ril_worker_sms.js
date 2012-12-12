/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

const ESCAPE = "\uffff";
const RESCTL = "\ufffe";
const LF = "\n";
const CR = "\r";
const SP = " ";
const FF = "\u000c";

function run_test() {
  run_next_test();
}

/**
 * Verify validity of the national language tables
 */
add_test(function test_nl_locking_shift_tables_validity() {
  for (let lst = 0; lst < PDU_NL_LOCKING_SHIFT_TABLES.length; lst++) {
    do_print("Verifying PDU_NL_LOCKING_SHIFT_TABLES[" + lst + "]");

    let table = PDU_NL_LOCKING_SHIFT_TABLES[lst];

    // Make sure table length is 128, or it will break table lookup algorithm.
    do_check_eq(table.length, 128);

    // Make sure special values are preserved.
    do_check_eq(table[PDU_NL_EXTENDED_ESCAPE], ESCAPE);
    do_check_eq(table[PDU_NL_LINE_FEED], LF);
    do_check_eq(table[PDU_NL_CARRIAGE_RETURN], CR);
    do_check_eq(table[PDU_NL_SPACE], SP);
  }

  run_next_test();
});

add_test(function test_nl_single_shift_tables_validity() {
  for (let sst = 0; sst < PDU_NL_SINGLE_SHIFT_TABLES.length; sst++) {
    do_print("Verifying PDU_NL_SINGLE_SHIFT_TABLES[" + sst + "]");

    let table = PDU_NL_SINGLE_SHIFT_TABLES[sst];

    // Make sure table length is 128, or it will break table lookup algorithm.
    do_check_eq(table.length, 128);

    // Make sure special values are preserved.
    do_check_eq(table[PDU_NL_EXTENDED_ESCAPE], ESCAPE);
    do_check_eq(table[PDU_NL_PAGE_BREAK], FF);
    do_check_eq(table[PDU_NL_RESERVED_CONTROL], RESCTL);
  }

  run_next_test();
});

add_test(function test_gsm_sms_strict_7bit_charmap_validity() {
  let defaultTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
  for (let from in GSM_SMS_STRICT_7BIT_CHARMAP) {
    let to = GSM_SMS_STRICT_7BIT_CHARMAP[from];
    do_print("Verifying GSM_SMS_STRICT_7BIT_CHARMAP[\"\\u0x"
             + from.charCodeAt(0).toString(16) + "\"] => \"\\u"
             + to.charCodeAt(0).toString(16) + "\"");

    // Make sure "from" is not in default table
    do_check_eq(defaultTable.indexOf(from), -1);
    // Make sure "to" is in default table
    do_check_eq(defaultTable.indexOf(to) >= 0, true);
  }

  run_next_test();
});

/**
 * Verify GsmPDUHelper#readDataCodingScheme.
 */
add_test(function test_GsmPDUHelper_readDataCodingScheme() {
  let worker = newWorker({
    postRILMessage: function fakePostRILMessage(data) {
      // Do nothing
    },
    postMessage: function fakePostMessage(message) {
      // Do nothing
    }
  });

  let helper = worker.GsmPDUHelper;
  function test_dcs(dcs, encoding, messageClass, mwi) {
    helper.readHexOctet = function () {
      return dcs;
    }

    let msg = {};
    helper.readDataCodingScheme(msg);

    do_check_eq(msg.dcs, dcs);
    do_check_eq(msg.encoding, encoding);
    do_check_eq(msg.messageClass, messageClass);
    do_check_eq(msg.mwi == null, mwi == null);
    if (mwi != null) {
      do_check_eq(msg.mwi.active, mwi.active);
      do_check_eq(msg.mwi.discard, mwi.discard);
      do_check_eq(msg.mwi.msgCount, mwi.msgCount);
    }
  }

  // Group 00xx
  //   Bit 3 and 2 indicate the character set being used.
  test_dcs(0x00, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  test_dcs(0x04, PDU_DCS_MSG_CODING_8BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  test_dcs(0x08, PDU_DCS_MSG_CODING_16BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  test_dcs(0x0C, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  //   Bit 4, if set to 0, indicates that bits 1 to 0 are reserved and have no
  //   message class meaning.
  test_dcs(0x01, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  test_dcs(0x02, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  test_dcs(0x03, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  //   Bit 4, if set to 1, indicates that bits 1 to 0 have a message class meaning.
  test_dcs(0x10, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_0]);
  test_dcs(0x11, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_1]);
  test_dcs(0x12, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_2]);
  test_dcs(0x13, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_3]);

  // Group 01xx
  test_dcs(0x50, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_0]);

  // Group 1000..1011: reserved
  test_dcs(0x8F, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  test_dcs(0x9F, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  test_dcs(0xAF, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  test_dcs(0xBF, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);

  // Group 1100: Message Waiting Indication Group: Discard Message
  //   Bit 3 indicates Indication Sense:
  test_dcs(0xC0, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
           {active: false, discard: true, msgCount: 0});
  test_dcs(0xC8, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
           {active: true, discard: true, msgCount: -1});
  //   Bit 2 is reserved, and set to 0:
  test_dcs(0xCC, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
           {active: true, discard: true, msgCount: -1});

  // Group 1101: Message Waiting Indication Group: Store Message
  //   Bit 3 indicates Indication Sense:
  test_dcs(0xD0, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
           {active: false, discard: false, msgCount: 0});
  test_dcs(0xD8, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
           {active: true, discard: false, msgCount: -1});
  //   Bit 2 is reserved, and set to 0:
  test_dcs(0xDC, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
           {active: true, discard: false, msgCount: -1});

  // Group 1110: Message Waiting Indication Group: Store Message, UCS2
  //   Bit 3 indicates Indication Sense:
  test_dcs(0xE0, PDU_DCS_MSG_CODING_16BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
           {active: false, discard: false, msgCount: 0});
  test_dcs(0xE8, PDU_DCS_MSG_CODING_16BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
           {active: true, discard: false, msgCount: -1});
  //   Bit 2 is reserved, and set to 0:
  test_dcs(0xEC, PDU_DCS_MSG_CODING_16BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
           {active: true, discard: false, msgCount: -1});

  // Group 1111
  test_dcs(0xF0, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_0]);
  test_dcs(0xF1, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_1]);
  test_dcs(0xF2, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_2]);
  test_dcs(0xF3, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_3]);
  test_dcs(0xF4, PDU_DCS_MSG_CODING_8BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_0]);
  test_dcs(0xF5, PDU_DCS_MSG_CODING_8BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_1]);
  test_dcs(0xF6, PDU_DCS_MSG_CODING_8BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_2]);
  test_dcs(0xF7, PDU_DCS_MSG_CODING_8BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_3]);
  //   Bit 3 is reserved and should be set to 0, but if it doesn't we should
  //   ignore it.
  test_dcs(0xF8, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_0]);

  run_next_test();
});

/**
 * Verify RadioInterfaceLayer#_countGsm7BitSeptets() and
 * GsmPDUHelper#writeStringAsSeptets() algorithm match each other.
 */
add_test(function test_RadioInterfaceLayer__countGsm7BitSeptets() {
  let ril = newRadioInterfaceLayer();

  let worker = newWorker({
    postRILMessage: function fakePostRILMessage(data) {
      // Do nothing
    },
    postMessage: function fakePostMessage(message) {
      // Do nothing
    }
  });

  let helper = worker.GsmPDUHelper;
  helper.resetOctetWritten = function () {
    helper.octetsWritten = 0;
  };
  helper.writeHexOctet = function () {
    helper.octetsWritten++;
  };

  function do_check_calc(str, expectedCalcLen, lst, sst, strict7BitEncoding) {
    do_check_eq(expectedCalcLen,
                ril._countGsm7BitSeptets(str,
                                         PDU_NL_LOCKING_SHIFT_TABLES[lst],
                                         PDU_NL_SINGLE_SHIFT_TABLES[sst],
                                         strict7BitEncoding));

    helper.resetOctetWritten();
    helper.writeStringAsSeptets(str, 0, lst, sst, strict7BitEncoding);
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
  let str = "\u00c1\u00e1\u00cd\u00ed\u00d3\u00f3\u00da\u00fa\u00e7";
  do_check_calc(str, str.length, PDU_NL_IDENTIFIER_DEFAULT, PDU_NL_IDENTIFIER_DEFAULT, true);

  run_next_test();
});

/**
 * Verify RadioInterfaceLayer#calculateUserDataLength handles national language
 * selection correctly.
 */
add_test(function test_RadioInterfaceLayer__calculateUserDataLength() {
  let ril = newRadioInterfaceLayer();

  function test_calc(str, expected, enabledGsmTableTuples) {
    ril.enabledGsmTableTuples = enabledGsmTableTuples;
    let options = ril._calculateUserDataLength(str, expected[5]);

    do_check_eq(str, options.fullBody);
    do_check_eq(expected[0], options.dcs);
    do_check_eq(expected[1], options.encodedFullBodyLength);
    do_check_eq(expected[2], options.userDataHeaderLength);
    do_check_eq(expected[3], options.langIndex);
    do_check_eq(expected[4], options.langShiftIndex);
    do_check_eq(expected[5], options.strict7BitEncoding);
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
  let str = "\u00c1\u00e1\u00cd\u00ed\u00d3\u00f3\u00da\u00fa\u00e7";
  test_calc(str, [PDU_DCS_MSG_CODING_7BITS_ALPHABET, str.length, 0, 0, 0, true], [[0, 0]]);
  test_calc(str, [PDU_DCS_MSG_CODING_16BITS_ALPHABET, str.length * 2, 0], [[0, 0]]);

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
 * Verify RadioInterfaceLayer#_calculateUserDataLength7Bit multipart handling.
 */
add_test(function test_RadioInterfaceLayer__calculateUserDataLength7Bit_multipart() {
  let ril = newRadioInterfaceLayer();

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
 * Verify RadioInterfaceLayer#_fragmentText().
 */
add_test(function test_RadioInterfaceLayer__fragmentText7Bit() {
  let ril = newRadioInterfaceLayer();

  function test_calc(str, strict7BitEncoding, expected) {
    let options = ril._fragmentText(str, null, strict7BitEncoding);
    if (expected) {
      do_check_eq(expected, options.segments.length);
    } else {
      do_check_eq(null, options.segments);
    }
  }

  // 7-Bit

  // Boundary checks
  test_calc("", false);
  test_calc("", true);
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
  test_calc(generateStringOfLength("\ua2db", PDU_MAX_USER_DATA_UCS2 + 1), true, 2);

  // UCS2 character cannot be separated
  ril.segmentRef16Bit = true;
  test_calc(generateStringOfLength("\ua2db", (PDU_MAX_USER_DATA_UCS2 * 2 - 7) * 2 / 2), false, 3);
  ril.segmentRef16Bit = false;

  // Test Bug 790192: support strict GSM SMS 7-Bit encoding
  let str = "\u00c1\u00e1\u00cd\u00ed\u00d3\u00f3\u00da\u00fa\u00e7";
  for (let i = 0; i < str.length; i++) {
    let c = str.charAt(i);
    test_calc(generateStringOfLength(c, PDU_MAX_USER_DATA_7BIT), false, 3);
    test_calc(generateStringOfLength(c, PDU_MAX_USER_DATA_7BIT), true);
    test_calc(generateStringOfLength(c, PDU_MAX_USER_DATA_UCS2), false);
  }

  run_next_test();
});

/**
 * Verify GsmPDUHelper#writeStringAsSeptets() padding bits handling.
 */
add_test(function test_GsmPDUHelper_writeStringAsSeptets() {
  let worker = newWorker({
    postRILMessage: function fakePostRILMessage(data) {
      // Do nothing
    },
    postMessage: function fakePostMessage(message) {
      // Do nothing
    }
  });

  let helper = worker.GsmPDUHelper;
  helper.resetOctetWritten = function () {
    helper.octetsWritten = 0;
  };
  helper.writeHexOctet = function () {
    helper.octetsWritten++;
  };

  let base = "AAAAAAAA"; // Base string of 8 characters long
  for (let len = 0; len < 8; len++) {
    let str = base.substring(0, len);

    for (let paddingBits = 0; paddingBits < 8; paddingBits++) {
      do_print("Verifying GsmPDUHelper.writeStringAsSeptets("
               + str + ", " + paddingBits + ", <default>, <default>)");
      helper.resetOctetWritten();
      helper.writeStringAsSeptets(str, paddingBits, PDU_NL_IDENTIFIER_DEFAULT,
                                  PDU_NL_IDENTIFIER_DEFAULT);
      do_check_eq(Math.ceil(((len * 7) + paddingBits) / 8),
                  helper.octetsWritten);
    }
  }

  // Test Bug 790192: support strict GSM SMS 7-Bit encoding
  let str = "\u00c1\u00e1\u00cd\u00ed\u00d3\u00f3\u00da\u00fa\u00e7";
  helper.resetOctetWritten();
  do_print("Verifying GsmPDUHelper.writeStringAsSeptets(" + str + ", 0, <default>, <default>, true)");
  helper.writeStringAsSeptets(str, 0, PDU_NL_IDENTIFIER_DEFAULT, PDU_NL_IDENTIFIER_DEFAULT, true);
  do_check_eq(Math.ceil(str.length * 7 / 8), helper.octetsWritten);

  run_next_test();
});

/**
 * Verify receiving SMS-DELIVERY messages
 */

function hexToNibble(nibble) {
  nibble &= 0x0f;
  if (nibble < 10) {
    nibble += 48; // ASCII '0'
  } else {
    nibble += 55; // ASCII 'A'
  }
  return nibble;
}

function pduToParcelData(pdu) {
  let dataLength = 4 + pdu.length * 4 + 4;
  let data = new Uint8Array(dataLength);
  let offset = 0;

  // String length
  data[offset++] = pdu.length & 0xFF;
  data[offset++] = (pdu.length >> 8) & 0xFF;
  data[offset++] = (pdu.length >> 16) & 0xFF;
  data[offset++] = (pdu.length >> 24) & 0xFF;

  // PDU data
  for (let i = 0; i < pdu.length; i++) {
    let hi = (pdu[i] >>> 4) & 0x0F;
    let lo = pdu[i] & 0x0F;

    data[offset++] = hexToNibble(hi);
    data[offset++] = 0;
    data[offset++] = hexToNibble(lo);
    data[offset++] = 0;
  }

  // String delimitor
  data[offset++] = 0;
  data[offset++] = 0;
  data[offset++] = 0;
  data[offset++] = 0;

  return data;
}

function compose7bitPdu(lst, sst, data, septets) {
  if ((lst == 0) && (sst == 0)) {
    return [0x00,                              // SMSC
            PDU_MTI_SMS_DELIVER,               // firstOctet
            1, 0x00, 0,                        // senderAddress
            0x00,                              // protocolIdentifier
            PDU_DCS_MSG_CODING_7BITS_ALPHABET, // dataCodingScheme
            0, 0, 0, 0, 0, 0, 0,               // y m d h m s tz
            septets]                           // userDataLength
           .concat(data);
  }

  return [0x00,                                            // SMSC
          PDU_MTI_SMS_DELIVER | PDU_UDHI,                  // firstOctet
          1, 0x00, 0,                                      // senderAddress
          0x00,                                            // protocolIdentifier
          PDU_DCS_MSG_CODING_7BITS_ALPHABET,               // dataCodingScheme
          0, 0, 0, 0, 0, 0, 0,                             // y m d h m s tz
          8 + septets,                                     // userDataLength
          6,                                               // user data header length
          PDU_IEI_NATIONAL_LANGUAGE_LOCKING_SHIFT, 1, lst, // PDU_IEI_NATIONAL_LANGUAGE_LOCKING_SHIFT
          PDU_IEI_NATIONAL_LANGUAGE_SINGLE_SHIFT, 1, sst]  // PDU_IEI_NATIONAL_LANGUAGE_SINGLE_SHIFT
         .concat(data);
}

function composeUcs2Pdu(rawBytes) {
  return [0x00,                               // SMSC
          PDU_MTI_SMS_DELIVER,                // firstOctet
          1, 0x00, 0,                         // senderAddress
          0x00,                               // protocolIdentifier
          PDU_DCS_MSG_CODING_16BITS_ALPHABET, // dataCodingScheme
          0, 0, 0, 0, 0, 0, 0,                // y m d h m s tz
          rawBytes.length]                    // userDataLength
         .concat(rawBytes);
}

function newSmsParcel(pdu) {
  return newIncomingParcel(-1,
                           RESPONSE_TYPE_UNSOLICITED,
                           UNSOLICITED_RESPONSE_NEW_SMS,
                           pduToParcelData(pdu));
}

function removeSpecialChar(str, needle) {
  for (let i = 0; i < needle.length; i++) {
    let pos;
    while ((pos = str.indexOf(needle[i])) >= 0) {
      str = str.substring(0, pos) + str.substring(pos + 1);
    }
  }
  return str;
}

function newWriteHexOctetAsUint8Worker() {
  let worker = newWorker({
    postRILMessage: function fakePostRILMessage(data) {
      // Do nothing
    },
    postMessage: function fakePostMessage(message) {
      // Do nothing
    }
  });

  worker.GsmPDUHelper.writeHexOctet = function (value) {
    worker.Buf.writeUint8(value);
  };

  return worker;
}

function add_test_receiving_sms(expected, pdu) {
  add_test(function test_receiving_sms() {
    let worker = newWorker({
      postRILMessage: function fakePostRILMessage(data) {
        // Do nothing
      },
      postMessage: function fakePostMessage(message) {
        do_print("body: " + message.body);
        do_check_eq(expected, message.body)
      }
    });

    do_print("expect: " + expected);
    do_print("pdu: " + pdu);
    worker.onRILMessage(newSmsParcel(pdu));

    run_next_test();
  });
}

function test_receiving_7bit_alphabets(lst, sst) {
  let ril = newRadioInterfaceLayer();

  let worker = newWriteHexOctetAsUint8Worker();
  let helper = worker.GsmPDUHelper;
  let buf = worker.Buf;

  function get7bitRawBytes(expected) {
    buf.outgoingIndex = 0;
    helper.writeStringAsSeptets(expected, 0, lst, sst);

    let subArray = buf.outgoingBytes.subarray(0, buf.outgoingIndex);
    return Array.slice(subArray);
  }

  let langTable = PDU_NL_LOCKING_SHIFT_TABLES[lst];
  let langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[sst];

  let text = removeSpecialChar(langTable + langShiftTable, ESCAPE + RESCTL);
  for (let i = 0; i < text.length;) {
    let len = Math.min(70, text.length - i);
    let expected = text.substring(i, i + len);
    let septets = ril._countGsm7BitSeptets(expected, langTable, langShiftTable);
    let rawBytes = get7bitRawBytes(expected);
    let pdu = compose7bitPdu(lst, sst, rawBytes, septets);
    add_test_receiving_sms(expected, pdu);

    i += len;
  }
}

function test_receiving_ucs2_alphabets(text) {
  let worker = newWriteHexOctetAsUint8Worker();
  let buf = worker.Buf;

  function getUCS2RawBytes(expected) {
    buf.outgoingIndex = 0;
    worker.GsmPDUHelper.writeUCS2String(expected);

    let subArray = buf.outgoingBytes.subarray(0, buf.outgoingIndex);
    return Array.slice(subArray);
  }

  for (let i = 0; i < text.length;) {
    let len = Math.min(70, text.length - i);
    let expected = text.substring(i, i + len);
    let rawBytes = getUCS2RawBytes(expected);
    let pdu = composeUcs2Pdu(rawBytes);
    add_test_receiving_sms(expected, pdu);

    i += len;
  }
}

let ucs2str = "";
for (let lst = 0; lst < PDU_NL_LOCKING_SHIFT_TABLES.length; lst++) {
  ucs2str += PDU_NL_LOCKING_SHIFT_TABLES[lst];
  for (let sst = 0; sst < PDU_NL_SINGLE_SHIFT_TABLES.length; sst++) {
    test_receiving_7bit_alphabets(lst, sst);

    if (lst == 0) {
      ucs2str += PDU_NL_SINGLE_SHIFT_TABLES[sst];
    }
  }
}
test_receiving_ucs2_alphabets(ucs2str);

// Bug 820220: B2G SMS: wrong order and truncated content in multi-part messages
add_test(function test_sendSMS_UCS2_without_langIndex_langShiftIndex_defined() {
  let worker = newWriteHexOctetAsUint8Worker();

  worker.Buf.sendParcel = function () {
    // Each sendParcel() call represents one outgoing segment of a multipart
    // SMS message. Here, we have the first segment send, so it's "Hello "
    // only.
    //
    // 4(parcel size) + 4(request type) + 4(token)
    // + 4(two messages) + 4(null SMSC) + 4(message string length)
    // + 1(first octet) + 1(message reference)
    // + 2(DA len, TOA) + 4(addr)
    // + 1(pid) + 1(dcs)
    // + 1(UDL) + 6(UDHL, type, len, ref, max, seq)
    // + 12(2 * strlen("Hello "))
    // + 4(two delimitors) = 57
    //
    // If we have additional 6(type, len, langIndex, type len, langShiftIndex)
    // octets here, then bug 809553 is not fixed.
    do_check_eq(this.outgoingIndex, 57);

    run_next_test();
  };

  worker.RIL.sendSMS({
    number: "1",
    segmentMaxSeq: 2,
    fullBody: "Hello World!",
    dcs: PDU_DCS_MSG_CODING_16BITS_ALPHABET,
    segmentRef16Bit: false,
    userDataHeaderLength: 5,
    strict7BitEncoding: false,
    requestStatusReport: true,
    segments: [
      {
        body: "Hello ",
        encodedBodyLength: 12,
      }, {
        body: "World!",
        encodedBodyLength: 12,
      }
    ],
  });
});

/**
 * Verify GsmPDUHelper#readAddress
 */
add_test(function test_GsmPDUHelper_readAddress() {
  let worker = newWorker({
    postRILMessage: function fakePostRILMessage(data) {
      // Do nothing
    },
    postMessage: function fakePostMessage(message) {
      // Do nothing
    }

  });

  let helper = worker.GsmPDUHelper;
  function test_address(addrHex, addrString) {
    let uint16Array = [];
    let ix = 0;
    for (let i = 0; i < addrHex.length; ++i) {
      uint16Array[i] = addrHex[i].charCodeAt();
    }

    worker.Buf.readUint16 = function (){
      if(ix >= uint16Array.length) {
        do_throw("out of range in uint16Array");
      }
      return uint16Array[ix++];
    }
    let length = helper.readHexOctet();
    let parsedAddr = helper.readAddress(length);
    do_check_eq(parsedAddr, addrString);
  }

  // For AlphaNumeric
  test_address("04D01100", "_@");
  test_address("04D01000", "\u0394@");

  // Direct prepand
  test_address("0B914151245584F6", "+14154255486");
  test_address("0E914151245584B633", "+14154255486#33");

  // PDU_TOA_NATIONAL
  test_address("0BA14151245584F6", "14154255486");

  run_next_test();
});
