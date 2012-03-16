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

  function do_check_calc(str, expectedCalcLen, lst, sst) {
    do_check_eq(expectedCalcLen,
                ril._countGsm7BitSeptets(str,
                                         PDU_NL_LOCKING_SHIFT_TABLES[lst],
                                         PDU_NL_SINGLE_SHIFT_TABLES[sst]));

    helper.resetOctetWritten();
    helper.writeStringAsSeptets(str, 0, lst, sst);
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
    let options = ril._calculateUserDataLength(str);

    do_check_eq(expected[0], options.dcs);
    do_check_eq(expected[1], options.encodedBodyLength);
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

