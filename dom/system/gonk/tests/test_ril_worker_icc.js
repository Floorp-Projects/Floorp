/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

/**
 * Helper function.
 */
function newUint8Worker() {
  let worker = newWorker();
  let index = 0; // index for read
  let buf = [];

  let context = worker.ContextPool._contexts[0];
  context.Buf.writeUint8 = function(value) {
    buf.push(value);
  };

  context.Buf.readUint8 = function() {
    return buf[index++];
  };

  context.Buf.seekIncoming = function(offset) {
    index += offset;
  };

  context.Buf.getReadAvailable = function() {
    return buf.length - index;
  };

  worker.debug = do_print;

  return worker;
}

/**
 * Verify ICCPDUHelper#readICCUCS2String()
 */
add_test(function test_read_icc_ucs2_string() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let iccHelper = context.ICCPDUHelper;

  // 0x80
  let text = "TEST";
  helper.writeUCS2String(text);
  // Also write two unused octets.
  let ffLen = 2;
  for (let i = 0; i < ffLen; i++) {
    helper.writeHexOctet(0xff);
  }
  do_check_eq(iccHelper.readICCUCS2String(0x80, (2 * text.length) + ffLen), text);

  // 0x81
  let array = [0x08, 0xd2, 0x4d, 0x6f, 0x7a, 0x69, 0x6c, 0x6c, 0x61, 0xca,
               0xff, 0xff];
  let len = array.length;
  for (let i = 0; i < len; i++) {
    helper.writeHexOctet(array[i]);
  }
  do_check_eq(iccHelper.readICCUCS2String(0x81, len), "Mozilla\u694a");

  // 0x82
  let array2 = [0x08, 0x69, 0x00, 0x4d, 0x6f, 0x7a, 0x69, 0x6c, 0x6c, 0x61,
                0xca, 0xff, 0xff];
  let len2 = array2.length;
  for (let i = 0; i < len2; i++) {
    helper.writeHexOctet(array2[i]);
  }
  do_check_eq(iccHelper.readICCUCS2String(0x82, len2), "Mozilla\u694a");

  run_next_test();
});

/**
 * Verify ICCPDUHelper#readDiallingNumber
 */
add_test(function test_read_dialling_number() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let iccHelper = context.ICCPDUHelper;
  let str = "123456789";

  helper.readHexOctet = function() {
    return 0x81;
  };

  helper.readSwappedNibbleBcdString = function(len) {
    return str.substring(0, len);
  };

  for (let i = 0; i < str.length; i++) {
    do_check_eq(str.substring(0, i - 1), // -1 for the TON
                iccHelper.readDiallingNumber(i));
  }

  run_next_test();
});

/**
 * Verify ICCPDUHelper#read8BitUnpackedToString
 */
add_test(function test_read_8bit_unpacked_to_string() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let iccHelper = context.ICCPDUHelper;
  const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
  const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];

  // Test 1: Read GSM alphabets.
  // Write alphabets before ESCAPE.
  for (let i = 0; i < PDU_NL_EXTENDED_ESCAPE; i++) {
    helper.writeHexOctet(i);
  }

  // Write two ESCAPEs to make it become ' '.
  helper.writeHexOctet(PDU_NL_EXTENDED_ESCAPE);
  helper.writeHexOctet(PDU_NL_EXTENDED_ESCAPE);

  for (let i = PDU_NL_EXTENDED_ESCAPE + 1; i < langTable.length; i++) {
    helper.writeHexOctet(i);
  }

  // Also write two unused fields.
  let ffLen = 2;
  for (let i = 0; i < ffLen; i++) {
    helper.writeHexOctet(0xff);
  }

  do_check_eq(iccHelper.read8BitUnpackedToString(PDU_NL_EXTENDED_ESCAPE),
              langTable.substring(0, PDU_NL_EXTENDED_ESCAPE));
  do_check_eq(iccHelper.read8BitUnpackedToString(2), " ");
  do_check_eq(iccHelper.read8BitUnpackedToString(langTable.length -
                                              PDU_NL_EXTENDED_ESCAPE - 1 + ffLen),
              langTable.substring(PDU_NL_EXTENDED_ESCAPE + 1));

  // Test 2: Read GSM extended alphabets.
  for (let i = 0; i < langShiftTable.length; i++) {
    helper.writeHexOctet(PDU_NL_EXTENDED_ESCAPE);
    helper.writeHexOctet(i);
  }

  // Read string before RESERVED_CONTROL.
  do_check_eq(iccHelper.read8BitUnpackedToString(PDU_NL_RESERVED_CONTROL  * 2),
              langShiftTable.substring(0, PDU_NL_RESERVED_CONTROL));
  // ESCAPE + RESERVED_CONTROL will become ' '.
  do_check_eq(iccHelper.read8BitUnpackedToString(2), " ");
  // Read string between RESERVED_CONTROL and EXTENDED_ESCAPE.
  do_check_eq(iccHelper.read8BitUnpackedToString(
                (PDU_NL_EXTENDED_ESCAPE - PDU_NL_RESERVED_CONTROL - 1)  * 2),
              langShiftTable.substring(PDU_NL_RESERVED_CONTROL + 1,
                                       PDU_NL_EXTENDED_ESCAPE));
  // ESCAPE + ESCAPE will become ' '.
  do_check_eq(iccHelper.read8BitUnpackedToString(2), " ");
  // Read remaining string.
  do_check_eq(iccHelper.read8BitUnpackedToString(
                (langShiftTable.length - PDU_NL_EXTENDED_ESCAPE - 1)  * 2),
              langShiftTable.substring(PDU_NL_EXTENDED_ESCAPE + 1));

  run_next_test();
});

/**
 * Verify ICCPDUHelper#writeStringTo8BitUnpacked.
 *
 * Test writing GSM 8 bit alphabets.
 */
add_test(function test_write_string_to_8bit_unpacked() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let iccHelper = context.ICCPDUHelper;
  const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
  const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
  // Length of trailing 0xff.
  let ffLen = 2;
  let str;

  // Test 1, write GSM alphabets.
  iccHelper.writeStringTo8BitUnpacked(langTable.length + ffLen, langTable);

  for (let i = 0; i < langTable.length; i++) {
    do_check_eq(helper.readHexOctet(), i);
  }

  for (let i = 0; i < ffLen; i++) {
    do_check_eq(helper.readHexOctet(), 0xff);
  }

  // Test 2, write GSM extended alphabets.
  str = "\u000c\u20ac";
  iccHelper.writeStringTo8BitUnpacked(4, str);

  do_check_eq(iccHelper.read8BitUnpackedToString(4), str);

  // Test 3, write GSM and GSM extended alphabets.
  // \u000c, \u20ac are from gsm extended alphabets.
  // \u00a3 is from gsm alphabet.
  str = "\u000c\u20ac\u00a3";

  // 2 octets * 2 = 4 octets for 2 gsm extended alphabets,
  // 1 octet for 1 gsm alphabet,
  // 2 octes for trailing 0xff.
  // "Totally 7 octets are to be written."
  iccHelper.writeStringTo8BitUnpacked(7, str);

  do_check_eq(iccHelper.read8BitUnpackedToString(7), str);

  run_next_test();
});

/**
 * Verify ICCPDUHelper#writeStringTo8BitUnpacked with maximum octets written.
 */
add_test(function test_write_string_to_8bit_unpacked_with_max_octets_written() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let iccHelper = context.ICCPDUHelper;
  const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
  const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];

  // The maximum of the number of octets that can be written is 3.
  // Only 3 characters shall be written even the length of the string is 4.
  iccHelper.writeStringTo8BitUnpacked(3, langTable.substring(0, 4));
  helper.writeHexOctet(0xff); // dummy octet.
  for (let i = 0; i < 3; i++) {
    do_check_eq(helper.readHexOctet(), i);
  }
  do_check_false(helper.readHexOctet() == 4);

  // \u000c is GSM extended alphabet, 2 octets.
  // \u00a3 is GSM alphabet, 1 octet.
  let str = "\u000c\u00a3";
  iccHelper.writeStringTo8BitUnpacked(3, str);
  do_check_eq(iccHelper.read8BitUnpackedToString(3), str);

  str = "\u00a3\u000c";
  iccHelper.writeStringTo8BitUnpacked(3, str);
  do_check_eq(iccHelper.read8BitUnpackedToString(3), str);

  // 2 GSM extended alphabets cost 4 octets, but maximum is 3, so only the 1st
  // alphabet can be written.
  str = "\u000c\u000c";
  iccHelper.writeStringTo8BitUnpacked(3, str);
  helper.writeHexOctet(0xff); // dummy octet.
  do_check_eq(iccHelper.read8BitUnpackedToString(4), str.substring(0, 1));

  run_next_test();
});

/**
 * Verify ICCPDUHelper.readAlphaIdentifier
 */
add_test(function test_read_alpha_identifier() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let iccHelper = context.ICCPDUHelper;

  // UCS2: 0x80
  let text = "TEST";
  helper.writeHexOctet(0x80);
  helper.writeUCS2String(text);
  // Also write two unused octets.
  let ffLen = 2;
  for (let i = 0; i < ffLen; i++) {
    helper.writeHexOctet(0xff);
  }
  do_check_eq(iccHelper.readAlphaIdentifier(1 + (2 * text.length) + ffLen), text);

  // UCS2: 0x81
  let array = [0x81, 0x08, 0xd2, 0x4d, 0x6f, 0x7a, 0x69, 0x6c, 0x6c, 0x61, 0xca, 0xff, 0xff];
  for (let i = 0; i < array.length; i++) {
    helper.writeHexOctet(array[i]);
  }
  do_check_eq(iccHelper.readAlphaIdentifier(array.length), "Mozilla\u694a");

  // UCS2: 0x82
  let array2 = [0x82, 0x08, 0x69, 0x00, 0x4d, 0x6f, 0x7a, 0x69, 0x6c, 0x6c, 0x61, 0xca, 0xff, 0xff];
  for (let i = 0; i < array2.length; i++) {
    helper.writeHexOctet(array2[i]);
  }
  do_check_eq(iccHelper.readAlphaIdentifier(array2.length), "Mozilla\u694a");

  // GSM 8 Bit Unpacked
  const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
  for (let i = 0; i < PDU_NL_EXTENDED_ESCAPE; i++) {
    helper.writeHexOctet(i);
  }
  do_check_eq(iccHelper.readAlphaIdentifier(PDU_NL_EXTENDED_ESCAPE),
              langTable.substring(0, PDU_NL_EXTENDED_ESCAPE));

  run_next_test();
});

/**
 * Verify ICCPDUHelper.writeAlphaIdentifier
 */
add_test(function test_write_alpha_identifier() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let iccHelper = context.ICCPDUHelper;
  // Length of trailing 0xff.
  let ffLen = 2;

  // Removal
  iccHelper.writeAlphaIdentifier(10, null);
  do_check_eq(iccHelper.readAlphaIdentifier(10), "");

  // GSM 8 bit
  let str = "Mozilla";
  iccHelper.writeAlphaIdentifier(str.length + ffLen, str);
  do_check_eq(iccHelper.readAlphaIdentifier(str.length + ffLen), str);

  // UCS2
  str = "Mozilla\u694a";
  iccHelper.writeAlphaIdentifier(str.length * 2 + ffLen, str);
  // * 2 for each character will be encoded to UCS2 alphabets.
  do_check_eq(iccHelper.readAlphaIdentifier(str.length * 2 + ffLen), str);

  // Test with maximum octets written.
  // 1 coding scheme (0x80) and 1 UCS2 character, total 3 octets.
  str = "\u694a";
  iccHelper.writeAlphaIdentifier(3, str);
  do_check_eq(iccHelper.readAlphaIdentifier(3), str);

  // 1 coding scheme (0x80) and 2 UCS2 characters, total 5 octets.
  // numOctets is limited to 4, so only 1 UCS2 character can be written.
  str = "\u694a\u694a";
  iccHelper.writeAlphaIdentifier(4, str);
  helper.writeHexOctet(0xff); // dummy octet.
  do_check_eq(iccHelper.readAlphaIdentifier(5), str.substring(0, 1));

  // Write 0 octet.
  iccHelper.writeAlphaIdentifier(0, "1");
  helper.writeHexOctet(0xff); // dummy octet.
  do_check_eq(iccHelper.readAlphaIdentifier(1), "");

  run_next_test();
});

/**
 * Verify ICCPDUHelper.readAlphaIdDiallingNumber
 */
add_test(function test_read_alpha_id_dialling_number() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let iccHelper = context.ICCPDUHelper;
  let buf = context.Buf;
  const recordSize = 32;

  function testReadAlphaIdDiallingNumber(contact) {
    iccHelper.readAlphaIdentifier = function() {
      return contact.alphaId;
    };

    iccHelper.readNumberWithLength = function() {
      return contact.number;
    };

    let strLen = recordSize * 2;
    buf.writeInt32(strLen);     // fake length
    helper.writeHexOctet(0xff); // fake CCP
    helper.writeHexOctet(0xff); // fake EXT1
    buf.writeStringDelimiter(strLen);

    let contactR = iccHelper.readAlphaIdDiallingNumber(recordSize);
    if (contact.alphaId == "" && contact.number == "") {
      do_check_eq(contactR, null);
    } else {
      do_check_eq(contactR.alphaId, contact.alphaId);
      do_check_eq(contactR.number, contact.number);
    }
  }

  testReadAlphaIdDiallingNumber({alphaId: "AlphaId", number: "0987654321"});
  testReadAlphaIdDiallingNumber({alphaId: "", number: ""});

  run_next_test();
});

/**
 * Verify ICCPDUHelper.writeAlphaIdDiallingNumber
 */
add_test(function test_write_alpha_id_dialling_number() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.ICCPDUHelper;
  const recordSize = 32;

  // Write a normal contact.
  let contactW = {
    alphaId: "Mozilla",
    number: "1234567890"
  };
  helper.writeAlphaIdDiallingNumber(recordSize, contactW.alphaId,
                                    contactW.number);

  let contactR = helper.readAlphaIdDiallingNumber(recordSize);
  do_check_eq(contactW.alphaId, contactR.alphaId);
  do_check_eq(contactW.number, contactR.number);

  // Write a contact with alphaId encoded in UCS2 and number has '+'.
  let contactUCS2 = {
    alphaId: "火狐",
    number: "+1234567890"
  };
  helper.writeAlphaIdDiallingNumber(recordSize, contactUCS2.alphaId,
                                    contactUCS2.number);
  contactR = helper.readAlphaIdDiallingNumber(recordSize);
  do_check_eq(contactUCS2.alphaId, contactR.alphaId);
  do_check_eq(contactUCS2.number, contactR.number);

  // Write a null contact (Removal).
  helper.writeAlphaIdDiallingNumber(recordSize);
  contactR = helper.readAlphaIdDiallingNumber(recordSize);
  do_check_eq(contactR, null);

  // Write a longer alphaId/dialling number
  // Dialling Number : Maximum 20 digits(10 octets).
  // Alpha Identifier: 32(recordSize) - 14 (10 octets for Dialling Number, 1
  //                   octet for TON/NPI, 1 for number length octet, and 2 for
  //                   Ext) = Maximum 18 octets.
  let longContact = {
    alphaId: "AAAAAAAAABBBBBBBBBCCCCCCCCC",
    number: "123456789012345678901234567890",
  };
  helper.writeAlphaIdDiallingNumber(recordSize, longContact.alphaId,
                                    longContact.number);
  contactR = helper.readAlphaIdDiallingNumber(recordSize);
  do_check_eq(contactR.alphaId, "AAAAAAAAABBBBBBBBB");
  do_check_eq(contactR.number, "12345678901234567890");

  // Add '+' to number and test again.
  longContact.number = "+123456789012345678901234567890";
  helper.writeAlphaIdDiallingNumber(recordSize, longContact.alphaId,
                                    longContact.number);
  contactR = helper.readAlphaIdDiallingNumber(recordSize);
  do_check_eq(contactR.alphaId, "AAAAAAAAABBBBBBBBB");
  do_check_eq(contactR.number, "+12345678901234567890");

  run_next_test();
});

/**
 * Verify ICCPDUHelper.writeDiallingNumber
 */
add_test(function test_write_dialling_number() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.ICCPDUHelper;

  // with +
  let number = "+123456";
  let len = 4;
  helper.writeDiallingNumber(number);
  do_check_eq(helper.readDiallingNumber(len), number);

  // without +
  number = "987654";
  len = 4;
  helper.writeDiallingNumber(number);
  do_check_eq(helper.readDiallingNumber(len), number);

  number = "9876543";
  len = 5;
  helper.writeDiallingNumber(number);
  do_check_eq(helper.readDiallingNumber(len), number);

  run_next_test();
});

/**
 * Verify ICCPDUHelper.readNumberWithLength
 */
add_test(function test_read_number_with_length() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let iccHelper = context.ICCPDUHelper;
  let number = "123456789";

  iccHelper.readDiallingNumber = function(numLen) {
    return number.substring(0, numLen);
  };

  helper.writeHexOctet(number.length + 1);
  helper.writeHexOctet(PDU_TOA_ISDN);
  do_check_eq(iccHelper.readNumberWithLength(), number);

  helper.writeHexOctet(0xff);
  do_check_eq(iccHelper.readNumberWithLength(), null);

  run_next_test();
});

/**
 * Verify ICCPDUHelper.writeNumberWithLength
 */
add_test(function test_write_number_with_length() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let iccHelper = context.ICCPDUHelper;

  function test(number, expectedNumber) {
    expectedNumber = expectedNumber || number;
    iccHelper.writeNumberWithLength(number);
    let numLen = helper.readHexOctet();
    do_check_eq(expectedNumber, iccHelper.readDiallingNumber(numLen));
    for (let i = 0; i < (ADN_MAX_BCD_NUMBER_BYTES - numLen); i++) {
      do_check_eq(0xff, helper.readHexOctet());
    }
  }

  // without +
  test("123456789");

  // with +
  test("+987654321");

  // extended BCD coding
  test("1*2#3,4*5#6,");

  // with + and extended BCD coding
  test("+1*2#3,4*5#6,");

  // non-supported characters should not be written.
  test("(1)23-456+789", "123456789");

  test("++(01)2*3-4#5,6+7(8)9*0#1,", "+012*34#5,6789*0#1,");

  // null
  iccHelper.writeNumberWithLength(null);
  for (let i = 0; i < (ADN_MAX_BCD_NUMBER_BYTES + 1); i++) {
    do_check_eq(0xff, helper.readHexOctet());
  }

  run_next_test();
});

/**
 * Verify GsmPDUHelper.writeTimestamp
 */
add_test(function test_write_timestamp() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;

  // current date
  let dateInput = new Date();
  let dateOutput = new Date();
  helper.writeTimestamp(dateInput);
  dateOutput.setTime(helper.readTimestamp());

  do_check_eq(dateInput.getFullYear(), dateOutput.getFullYear());
  do_check_eq(dateInput.getMonth(), dateOutput.getMonth());
  do_check_eq(dateInput.getDate(), dateOutput.getDate());
  do_check_eq(dateInput.getHours(), dateOutput.getHours());
  do_check_eq(dateInput.getMinutes(), dateOutput.getMinutes());
  do_check_eq(dateInput.getSeconds(), dateOutput.getSeconds());
  do_check_eq(dateInput.getTimezoneOffset(), dateOutput.getTimezoneOffset());

  // 2034-01-23 12:34:56 -0800 GMT
  let time = Date.UTC(2034, 1, 23, 12, 34, 56);
  time = time - (8 * 60 * 60 * 1000);
  dateInput.setTime(time);
  helper.writeTimestamp(dateInput);
  dateOutput.setTime(helper.readTimestamp());

  do_check_eq(dateInput.getFullYear(), dateOutput.getFullYear());
  do_check_eq(dateInput.getMonth(), dateOutput.getMonth());
  do_check_eq(dateInput.getDate(), dateOutput.getDate());
  do_check_eq(dateInput.getHours(), dateOutput.getHours());
  do_check_eq(dateInput.getMinutes(), dateOutput.getMinutes());
  do_check_eq(dateInput.getSeconds(), dateOutput.getSeconds());
  do_check_eq(dateInput.getTimezoneOffset(), dateOutput.getTimezoneOffset());

  run_next_test();
});

/**
 * Verify GsmPDUHelper.octectToBCD and GsmPDUHelper.BCDToOctet
 */
add_test(function test_octect_BCD() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;

  // 23
  let number = 23;
  let octet = helper.BCDToOctet(number);
  do_check_eq(helper.octetToBCD(octet), number);

  // 56
  number = 56;
  octet = helper.BCDToOctet(number);
  do_check_eq(helper.octetToBCD(octet), number);

  // 0x23
  octet = 0x23;
  number = helper.octetToBCD(octet);
  do_check_eq(helper.BCDToOctet(number), octet);

  // 0x56
  octet = 0x56;
  number = helper.octetToBCD(octet);
  do_check_eq(helper.BCDToOctet(number), octet);

  run_next_test();
});

/**
 * Verify ICCUtilsHelper.isICCServiceAvailable.
 */
add_test(function test_is_icc_service_available() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ICCUtilsHelper = context.ICCUtilsHelper;
  let RIL = context.RIL;

  function test_table(sst, geckoService, simEnabled, usimEnabled) {
    RIL.iccInfoPrivate.sst = sst;
    RIL.appType = CARD_APPTYPE_SIM;
    do_check_eq(ICCUtilsHelper.isICCServiceAvailable(geckoService), simEnabled);
    RIL.appType = CARD_APPTYPE_USIM;
    do_check_eq(ICCUtilsHelper.isICCServiceAvailable(geckoService), usimEnabled);
  }

  test_table([0x08], "ADN", true, false);
  test_table([0x08], "FDN", false, false);
  test_table([0x08], "SDN", false, true);

  run_next_test();
});

/**
 * Verify ICCUtilsHelper.isGsm8BitAlphabet
 */
add_test(function test_is_gsm_8bit_alphabet() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ICCUtilsHelper = context.ICCUtilsHelper;
  const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
  const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];

  do_check_eq(ICCUtilsHelper.isGsm8BitAlphabet(langTable), true);
  do_check_eq(ICCUtilsHelper.isGsm8BitAlphabet(langShiftTable), true);
  do_check_eq(ICCUtilsHelper.isGsm8BitAlphabet("\uaaaa"), false);

  run_next_test();
});

/**
 * Verify RIL.iccGetCardLockState("fdn")
 */
add_test(function test_icc_get_card_lock_state_fdn() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;
  let buf = context.Buf;

  buf.sendParcel = function() {
    // Request Type.
    do_check_eq(this.readInt32(), REQUEST_QUERY_FACILITY_LOCK)

    // Token : we don't care.
    this.readInt32();

    // String Array Length.
    do_check_eq(this.readInt32(), ril.v5Legacy ? 3 : 4);

    // Facility.
    do_check_eq(this.readString(), ICC_CB_FACILITY_FDN);

    // Password.
    do_check_eq(this.readString(), "");

    // Service class.
    do_check_eq(this.readString(), (ICC_SERVICE_CLASS_VOICE |
                                    ICC_SERVICE_CLASS_DATA  |
                                    ICC_SERVICE_CLASS_FAX).toString());

    if (!ril.v5Legacy) {
      // AID. Ignore because it's from modem.
      this.readInt32();
    }

    run_next_test();
  };

  ril.iccGetCardLockState({lockType: "fdn"});
});

add_test(function test_get_network_name_from_icc() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let RIL = context.RIL;
  let ICCUtilsHelper = context.ICCUtilsHelper;

  function testGetNetworkNameFromICC(operatorData, expectedResult) {
    let result = ICCUtilsHelper.getNetworkNameFromICC(operatorData.mcc,
                                                      operatorData.mnc,
                                                      operatorData.lac);

    if (expectedResult == null) {
      do_check_eq(result, expectedResult);
    } else {
      do_check_eq(result.fullName, expectedResult.longName);
      do_check_eq(result.shortName, expectedResult.shortName);
    }
  }

  // Before EF_OPL and EF_PNN have been loaded.
  testGetNetworkNameFromICC({mcc: 123, mnc: 456, lac: 0x1000}, null);
  testGetNetworkNameFromICC({mcc: 321, mnc: 654, lac: 0x2000}, null);

  // Set HPLMN
  RIL.iccInfo.mcc = 123;
  RIL.iccInfo.mnc = 456;

  RIL.voiceRegistrationState = {
    cell: {
      gsmLocationAreaCode: 0x1000
    }
  };
  RIL.operator = {};

  // Set EF_PNN
  RIL.iccInfoPrivate = {
    PNN: [
      {"fullName": "PNN1Long", "shortName": "PNN1Short"},
      {"fullName": "PNN2Long", "shortName": "PNN2Short"},
      {"fullName": "PNN3Long", "shortName": "PNN3Short"},
      {"fullName": "PNN4Long", "shortName": "PNN4Short"}
    ]
  };

  // EF_OPL isn't available and current isn't in HPLMN,
  testGetNetworkNameFromICC({mcc: 321, mnc: 654, lac: 0x1000}, null);

  // EF_OPL isn't available and current is in HPLMN,
  // the first record of PNN should be returned.
  testGetNetworkNameFromICC({mcc: 123, mnc: 456, lac: 0x1000},
                            {longName: "PNN1Long", shortName: "PNN1Short"});

  // Set EF_OPL
  RIL.iccInfoPrivate.OPL = [
    {
      "mcc": 123,
      "mnc": 456,
      "lacTacStart": 0,
      "lacTacEnd": 0xFFFE,
      "pnnRecordId": 4
    },
    {
      "mcc": 321,
      "mnc": 654,
      "lacTacStart": 0,
      "lacTacEnd": 0x0010,
      "pnnRecordId": 3
    },
    {
      "mcc": 321,
      "mnc": 654,
      "lacTacStart": 0x0100,
      "lacTacEnd": 0x1010,
      "pnnRecordId": 2
    }
  ];

  // Both EF_PNN and EF_OPL are presented, and current PLMN is HPLMN,
  testGetNetworkNameFromICC({mcc: 123, mnc: 456, lac: 0x1000},
                            {longName: "PNN4Long", shortName: "PNN4Short"});

  // Current PLMN is not HPLMN, and according to LAC, we should get
  // the second PNN record.
  testGetNetworkNameFromICC({mcc: 321, mnc: 654, lac: 0x1000},
                            {longName: "PNN2Long", shortName: "PNN2Short"});

  // Current PLMN is not HPLMN, and according to LAC, we should get
  // the thrid PNN record.
  testGetNetworkNameFromICC({mcc: 321, mnc: 654, lac: 0x0001},
                            {longName: "PNN3Long", shortName: "PNN3Short"});

  run_next_test();
});

add_test(function test_path_id_for_spid_and_spn() {
  let worker = newWorker({
    postRILMessage: function(data) {
      // Do nothing
    },
    postMessage: function(message) {
      // Do nothing
    }});
  let context = worker.ContextPool._contexts[0];
  let RIL = context.RIL;
  let ICCFileHelper = context.ICCFileHelper;

  // Test SIM
  RIL.appType = CARD_APPTYPE_SIM;
  do_check_eq(ICCFileHelper.getEFPath(ICC_EF_SPDI),
              EF_PATH_MF_SIM + EF_PATH_DF_GSM);
  do_check_eq(ICCFileHelper.getEFPath(ICC_EF_SPN),
              EF_PATH_MF_SIM + EF_PATH_DF_GSM);

  // Test USIM
  RIL.appType = CARD_APPTYPE_USIM;
  do_check_eq(ICCFileHelper.getEFPath(ICC_EF_SPDI),
              EF_PATH_MF_SIM + EF_PATH_ADF_USIM);
  do_check_eq(ICCFileHelper.getEFPath(ICC_EF_SPDI),
              EF_PATH_MF_SIM + EF_PATH_ADF_USIM);
  run_next_test();
});

/**
 * Verify ICCUtilsHelper.parsePbrTlvs
 */
add_test(function test_parse_pbr_tlvs() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;

  let pbrTlvs = [
    {tag: ICC_USIM_TYPE1_TAG,
     length: 0x0F,
     value: [{tag: ICC_USIM_EFADN_TAG,
              length: 0x03,
              value: [0x4F, 0x3A, 0x02]},
             {tag: ICC_USIM_EFIAP_TAG,
              length: 0x03,
              value: [0x4F, 0x25, 0x01]},
             {tag: ICC_USIM_EFPBC_TAG,
              length: 0x03,
              value: [0x4F, 0x09, 0x04]}]
    },
    {tag: ICC_USIM_TYPE2_TAG,
     length: 0x05,
     value: [{tag: ICC_USIM_EFEMAIL_TAG,
              length: 0x03,
              value: [0x4F, 0x50, 0x0B]},
             {tag: ICC_USIM_EFANR_TAG,
              length: 0x03,
              value: [0x4F, 0x11, 0x02]},
             {tag: ICC_USIM_EFANR_TAG,
              length: 0x03,
              value: [0x4F, 0x12, 0x03]}]
    },
    {tag: ICC_USIM_TYPE3_TAG,
     length: 0x0A,
     value: [{tag: ICC_USIM_EFCCP1_TAG,
              length: 0x03,
              value: [0x4F, 0x3D, 0x0A]},
             {tag: ICC_USIM_EFEXT1_TAG,
              length: 0x03,
              value: [0x4F, 0x4A, 0x03]}]
    },
  ];

  let pbr = context.ICCUtilsHelper.parsePbrTlvs(pbrTlvs);
  do_check_eq(pbr.adn.fileId, 0x4F3a);
  do_check_eq(pbr.iap.fileId, 0x4F25);
  do_check_eq(pbr.pbc.fileId, 0x4F09);
  do_check_eq(pbr.email.fileId, 0x4F50);
  do_check_eq(pbr.anr0.fileId, 0x4f11);
  do_check_eq(pbr.anr1.fileId, 0x4f12);
  do_check_eq(pbr.ccp1.fileId, 0x4F3D);
  do_check_eq(pbr.ext1.fileId, 0x4F4A);

  run_next_test();
});

/**
 * Verify ICCIOHelper.loadLinearFixedEF with recordSize.
 */
add_test(function test_load_linear_fixed_ef() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;
  let io = context.ICCIOHelper;

  io.getResponse = function fakeGetResponse(options) {
    // When recordSize is provided, loadLinearFixedEF should call iccIO directly.
    do_check_true(false);
    run_next_test();
  };

  ril.iccIO = function fakeIccIO(options) {
    do_check_true(true);
    run_next_test();
  };

  io.loadLinearFixedEF({recordSize: 0x20});
});

/**
 * Verify ICCIOHelper.loadLinearFixedEF without recordSize.
 */
add_test(function test_load_linear_fixed_ef() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;
  let io = context.ICCIOHelper;

  io.getResponse = function fakeGetResponse(options) {
    do_check_true(true);
    run_next_test();
  };

  ril.iccIO = function fakeIccIO(options) {
    // When recordSize is not provided, loadLinearFixedEF should call getResponse.
    do_check_true(false);
    run_next_test();
  };

  io.loadLinearFixedEF({});
});

/**
 * Verify ICCRecordHelper.readPBR
 */
add_test(function test_read_pbr() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let record = context.ICCRecordHelper;
  let buf    = context.Buf;
  let io     = context.ICCIOHelper;

  io.loadLinearFixedEF = function fakeLoadLinearFixedEF(options) {
    let pbr_1 = [
      0xa8, 0x05, 0xc0, 0x03, 0x4f, 0x3a, 0x01
    ];

    // Write data size
    buf.writeInt32(pbr_1.length * 2);

    // Write pbr
    for (let i = 0; i < pbr_1.length; i++) {
      helper.writeHexOctet(pbr_1[i]);
    }

    // Write string delimiter
    buf.writeStringDelimiter(pbr_1.length * 2);

    options.totalRecords = 2;
    if (options.callback) {
      options.callback(options);
    }
  };

  io.loadNextRecord = function fakeLoadNextRecord(options) {
    let pbr_2 = [
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff
    ];

    options.p1++;
    if (options.callback) {
      options.callback(options);
    }
  };

  let successCb = function successCb(pbrs) {
    do_check_eq(pbrs[0].adn.fileId, 0x4f3a);
    do_check_eq(pbrs.length, 1);
  };

  let errorCb = function errorCb(errorMsg) {
    do_print("Reading EF_PBR failed, msg = " + errorMsg);
    do_check_true(false);
  };

  record.readPBR(successCb, errorCb);

  // Check cache pbrs when 2nd call
  let ifLoadEF = false;
  io.loadLinearFixedEF = function fakeLoadLinearFixedEF(options)  {
    ifLoadEF = true;
  }
  record.readPBR(successCb, errorCb);
  do_check_false(ifLoadEF);

  run_next_test();
});

/**
 * Verify ICCRecordHelper.readEmail
 */
add_test(function test_read_email() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let record = context.ICCRecordHelper;
  let buf    = context.Buf;
  let io     = context.ICCIOHelper;
  let recordSize;

  io.loadLinearFixedEF = function fakeLoadLinearFixedEF(options)  {
    let email_1 = [
      0x65, 0x6D, 0x61, 0x69, 0x6C,
      0x00, 0x6D, 0x6F, 0x7A, 0x69,
      0x6C, 0x6C, 0x61, 0x2E, 0x63,
      0x6F, 0x6D, 0x02, 0x23];

    // Write data size
    buf.writeInt32(email_1.length * 2);

    // Write email
    for (let i = 0; i < email_1.length; i++) {
      helper.writeHexOctet(email_1[i]);
    }

    // Write string delimiter
    buf.writeStringDelimiter(email_1.length * 2);

    recordSize = email_1.length;
    options.recordSize = recordSize;
    if (options.callback) {
      options.callback(options);
    }
  };

  function doTestReadEmail(type, expectedResult) {
    let fileId = 0x6a75;
    let recordNumber = 1;

    // fileId and recordNumber are dummy arguments.
    record.readEmail(fileId, type, recordNumber, function(email) {
      do_check_eq(email, expectedResult);
    });
  };

  doTestReadEmail(ICC_USIM_TYPE1_TAG, "email@mozilla.com$#");
  doTestReadEmail(ICC_USIM_TYPE2_TAG, "email@mozilla.com");
  do_check_eq(record._emailRecordSize, recordSize);

  run_next_test();
});

/**
 * Verify ICCRecordHelper.updateEmail
 */
add_test(function test_update_email() {
  const recordSize = 0x20;
  const recordNumber = 1;
  const fileId = 0x4f50;
  const NUM_TESTS = 2;
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let iccHelper = context.ICCPDUHelper;
  let ril = context.RIL;
  ril.appType = CARD_APPTYPE_USIM;
  let recordHelper = context.ICCRecordHelper;
  let buf = context.Buf;
  let ioHelper = context.ICCIOHelper;
  let pbr = {email: {fileId: fileId, fileType: ICC_USIM_TYPE1_TAG},
             adn: {sfi: 1}};
  let count = 0;

  // Override.
  ioHelper.updateLinearFixedEF = function(options) {
    options.pathId = context.ICCFileHelper.getEFPath(options.fileId);
    options.command = ICC_COMMAND_UPDATE_RECORD;
    options.p1 = options.recordNumber;
    options.p2 = READ_RECORD_ABSOLUTE_MODE;
    options.p3 = recordSize;
    ril.iccIO(options);
  };

  function do_test(pbr, expectedEmail, expectedAdnRecordId) {
    buf.sendParcel = function() {
      count++;

      // Request Type.
      do_check_eq(this.readInt32(), REQUEST_SIM_IO);

      // Token : we don't care
      this.readInt32();

      // command.
      do_check_eq(this.readInt32(), ICC_COMMAND_UPDATE_RECORD);

      // fileId.
      do_check_eq(this.readInt32(), fileId);

      // pathId.
      do_check_eq(this.readString(),
                  EF_PATH_MF_SIM + EF_PATH_DF_TELECOM + EF_PATH_DF_PHONEBOOK);

      // p1.
      do_check_eq(this.readInt32(), recordNumber);

      // p2.
      do_check_eq(this.readInt32(), READ_RECORD_ABSOLUTE_MODE);

      // p3.
      do_check_eq(this.readInt32(), recordSize);

      // data.
      let strLen = this.readInt32();
      let email;
      if (pbr.email.fileType === ICC_USIM_TYPE1_TAG) {
        email = iccHelper.read8BitUnpackedToString(recordSize);
      } else {
        email = iccHelper.read8BitUnpackedToString(recordSize - 2);
        do_check_eq(pduHelper.readHexOctet(), pbr.adn.sfi);
        do_check_eq(pduHelper.readHexOctet(), expectedAdnRecordId);
      }
      this.readStringDelimiter(strLen);
      do_check_eq(email, expectedEmail);

      // pin2.
      do_check_eq(this.readString(), null);

      if (!ril.v5Legacy) {
        // AID. Ignore because it's from modem.
        this.readInt32();
      }

      if (count == NUM_TESTS) {
        run_next_test();
      }
    };
    recordHelper.updateEmail(pbr, recordNumber, expectedEmail, expectedAdnRecordId);
  }

  do_test(pbr, "test@mail.com");
  pbr.email.fileType = ICC_USIM_TYPE2_TAG;
  do_test(pbr, "test@mail.com", 1);
});

/**
 * Verify ICCRecordHelper.readANR
 */
add_test(function test_read_anr() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let record = context.ICCRecordHelper;
  let buf    = context.Buf;
  let io     = context.ICCIOHelper;
  let recordSize;

  io.loadLinearFixedEF = function fakeLoadLinearFixedEF(options)  {
    let anr_1 = [
      0x01, 0x05, 0x81, 0x10, 0x32,
      0x54, 0xF6, 0xFF, 0xFF];

    // Write data size
    buf.writeInt32(anr_1.length * 2);

    // Write anr
    for (let i = 0; i < anr_1.length; i++) {
      helper.writeHexOctet(anr_1[i]);
    }

    // Write string delimiter
    buf.writeStringDelimiter(anr_1.length * 2);

    recordSize = anr_1.length;
    options.recordSize = recordSize;
    if (options.callback) {
      options.callback(options);
    }
  };

  function doTestReadAnr(fileType, expectedResult) {
    let fileId = 0x4f11;
    let recordNumber = 1;

    // fileId and recordNumber are dummy arguments.
    record.readANR(fileId, fileType, recordNumber, function(anr) {
      do_check_eq(anr, expectedResult);
    });
  };

  doTestReadAnr(ICC_USIM_TYPE1_TAG, "0123456");
  do_check_eq(record._anrRecordSize, recordSize);

  run_next_test();
});

/**
 * Verify ICCRecordHelper.updateANR
 */
add_test(function test_update_anr() {
  const recordSize = 0x20;
  const recordNumber = 1;
  const fileId = 0x4f11;
  const NUM_TESTS = 2;
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let iccHelper = context.ICCPDUHelper;
  let ril = context.RIL;
  ril.appType = CARD_APPTYPE_USIM;
  let recordHelper = context.ICCRecordHelper;
  let buf = context.Buf;
  let ioHelper = context.ICCIOHelper;
  let pbr = {anr0: {fileId: fileId, fileType: ICC_USIM_TYPE1_TAG},
             adn: {sfi: 1}};
  let count = 0;

  // Override.
  ioHelper.updateLinearFixedEF = function(options) {
    options.pathId = context.ICCFileHelper.getEFPath(options.fileId);
    options.command = ICC_COMMAND_UPDATE_RECORD;
    options.p1 = options.recordNumber;
    options.p2 = READ_RECORD_ABSOLUTE_MODE;
    options.p3 = recordSize;
    ril.iccIO(options);
  };

  function do_test(pbr, expectedANR, expectedAdnRecordId) {
    buf.sendParcel = function() {
      count++;

      // Request Type.
      do_check_eq(this.readInt32(), REQUEST_SIM_IO);

      // Token : we don't care
      this.readInt32();

      // command.
      do_check_eq(this.readInt32(), ICC_COMMAND_UPDATE_RECORD);

      // fileId.
      do_check_eq(this.readInt32(), fileId);

      // pathId.
      do_check_eq(this.readString(),
                  EF_PATH_MF_SIM + EF_PATH_DF_TELECOM + EF_PATH_DF_PHONEBOOK);

      // p1.
      do_check_eq(this.readInt32(), recordNumber);

      // p2.
      do_check_eq(this.readInt32(), READ_RECORD_ABSOLUTE_MODE);

      // p3.
      do_check_eq(this.readInt32(), recordSize);

      // data.
      let strLen = this.readInt32();
      // EF_AAS, ignore.
      pduHelper.readHexOctet();
      do_check_eq(iccHelper.readNumberWithLength(), expectedANR);
      // EF_CCP, ignore.
      pduHelper.readHexOctet();
      // EF_EXT1, ignore.
      pduHelper.readHexOctet();
      if (pbr.anr0.fileType === ICC_USIM_TYPE2_TAG) {
        do_check_eq(pduHelper.readHexOctet(), pbr.adn.sfi);
        do_check_eq(pduHelper.readHexOctet(), expectedAdnRecordId);
      }
      this.readStringDelimiter(strLen);

      // pin2.
      do_check_eq(this.readString(), null);

      if (!ril.v5Legacy) {
        // AID. Ignore because it's from modem.
        this.readInt32();
      }

      if (count == NUM_TESTS) {
        run_next_test();
      }
    };
    recordHelper.updateANR(pbr, recordNumber, expectedANR, expectedAdnRecordId);
  }

  do_test(pbr, "+123456789");
  pbr.anr0.fileType = ICC_USIM_TYPE2_TAG;
  do_test(pbr, "123456789", 1);
});

/**
 * Verify ICCRecordHelper.readIAP
 */
add_test(function test_read_iap() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let record = context.ICCRecordHelper;
  let buf    = context.Buf;
  let io     = context.ICCIOHelper;
  let recordSize;

  io.loadLinearFixedEF = function fakeLoadLinearFixedEF(options)  {
    let iap_1 = [0x01, 0x02];

    // Write data size/
    buf.writeInt32(iap_1.length * 2);

    // Write iap.
    for (let i = 0; i < iap_1.length; i++) {
      helper.writeHexOctet(iap_1[i]);
    }

    // Write string delimiter.
    buf.writeStringDelimiter(iap_1.length * 2);

    recordSize = iap_1.length;
    options.recordSize = recordSize;
    if (options.callback) {
      options.callback(options);
    }
  };

  function doTestReadIAP(expectedIAP) {
    const fileId = 0x4f17;
    const recordNumber = 1;

    let successCb = function successCb(iap) {
      for (let i = 0; i < iap.length; i++) {
        do_check_eq(expectedIAP[i], iap[i]);
      }
      run_next_test();
    }.bind(this);

    let errorCb = function errorCb(errorMsg) {
      do_print(errorMsg);
      do_check_true(false);
      run_next_test();
    }.bind(this);

    record.readIAP(fileId, recordNumber, successCb, errorCb);
  };

  doTestReadIAP([1, 2]);
});

/**
 * Verify ICCRecordHelper.updateIAP
 */
add_test(function test_update_iap() {
  const recordSize = 2;
  const recordNumber = 1;
  const fileId = 0x4f17;
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let ril = context.RIL;
  ril.appType = CARD_APPTYPE_USIM;
  let recordHelper = context.ICCRecordHelper;
  let buf = context.Buf;
  let ioHelper = context.ICCIOHelper;
  let count = 0;

  // Override.
  ioHelper.updateLinearFixedEF = function(options) {
    options.pathId = context.ICCFileHelper.getEFPath(options.fileId);
    options.command = ICC_COMMAND_UPDATE_RECORD;
    options.p1 = options.recordNumber;
    options.p2 = READ_RECORD_ABSOLUTE_MODE;
    options.p3 = recordSize;
    ril.iccIO(options);
  };

  function do_test(expectedIAP) {
    buf.sendParcel = function() {
      // Request Type.
      do_check_eq(this.readInt32(), REQUEST_SIM_IO);

      // Token : we don't care
      this.readInt32();

      // command.
      do_check_eq(this.readInt32(), ICC_COMMAND_UPDATE_RECORD);

      // fileId.
      do_check_eq(this.readInt32(), fileId);

      // pathId.
      do_check_eq(this.readString(),
                  EF_PATH_MF_SIM + EF_PATH_DF_TELECOM + EF_PATH_DF_PHONEBOOK);

      // p1.
      do_check_eq(this.readInt32(), recordNumber);

      // p2.
      do_check_eq(this.readInt32(), READ_RECORD_ABSOLUTE_MODE);

      // p3.
      do_check_eq(this.readInt32(), recordSize);

      // data.
      let strLen = this.readInt32();
      for (let i = 0; i < recordSize; i++) {
        do_check_eq(expectedIAP[i], pduHelper.readHexOctet());
      }
      this.readStringDelimiter(strLen);

      // pin2.
      do_check_eq(this.readString(), null);

      if (!ril.v5Legacy) {
        // AID. Ignore because it's from modem.
        this.readInt32();
      }

      run_next_test();
    };
    recordHelper.updateIAP(fileId, recordNumber, expectedIAP);
  }

  do_test([1, 2]);
});

/**
 * Verify ICCRecordHelper.updateADNLike.
 */
add_test(function test_update_adn_like() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;
  let record = context.ICCRecordHelper;
  let io = context.ICCIOHelper;
  let pdu = context.ICCPDUHelper;
  let buf = context.Buf;

  ril.appType = CARD_APPTYPE_SIM;
  const recordSize = 0x20;
  let fileId;

  // Override.
  io.updateLinearFixedEF = function(options) {
    options.pathId = context.ICCFileHelper.getEFPath(options.fileId);
    options.command = ICC_COMMAND_UPDATE_RECORD;
    options.p1 = options.recordNumber;
    options.p2 = READ_RECORD_ABSOLUTE_MODE;
    options.p3 = recordSize;
    ril.iccIO(options);
  };

  buf.sendParcel = function() {
    // Request Type.
    do_check_eq(this.readInt32(), REQUEST_SIM_IO);

    // Token : we don't care
    this.readInt32();

    // command.
    do_check_eq(this.readInt32(), ICC_COMMAND_UPDATE_RECORD);

    // fileId.
    do_check_eq(this.readInt32(), fileId);

    // pathId.
    do_check_eq(this.readString(), EF_PATH_MF_SIM + EF_PATH_DF_TELECOM);

    // p1.
    do_check_eq(this.readInt32(), 1);

    // p2.
    do_check_eq(this.readInt32(), READ_RECORD_ABSOLUTE_MODE);

    // p3.
    do_check_eq(this.readInt32(), 0x20);

    // data.
    let contact = pdu.readAlphaIdDiallingNumber(0x20);
    do_check_eq(contact.alphaId, "test");
    do_check_eq(contact.number, "123456");

    // pin2.
    if (fileId == ICC_EF_ADN) {
      do_check_eq(this.readString(), null);
    } else {
      do_check_eq(this.readString(), "1111");
    }

    if (!ril.v5Legacy) {
      // AID. Ignore because it's from modem.
      this.readInt32();
    }

    if (fileId == ICC_EF_FDN) {
      run_next_test();
    }
  };

  fileId = ICC_EF_ADN;
  record.updateADNLike(fileId,
                       {recordId: 1, alphaId: "test", number: "123456"});

  fileId = ICC_EF_FDN;
  record.updateADNLike(fileId,
                       {recordId: 1, alphaId: "test", number: "123456"},
                       "1111");
});

/**
 * Verify ICCRecordHelper.findFreeRecordId.
 */
add_test(function test_find_free_record_id() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let recordHelper = context.ICCRecordHelper;
  let buf = context.Buf;
  let io  = context.ICCIOHelper;

  function writeRecord (record) {
    // Write data size
    buf.writeInt32(record.length * 2);

    for (let i = 0; i < record.length; i++) {
      pduHelper.writeHexOctet(record[i]);
    }

    // Write string delimiter
    buf.writeStringDelimiter(record.length * 2);
  }

  io.loadLinearFixedEF = function fakeLoadLinearFixedEF(options)  {
    // Some random data.
    let record = [0x12, 0x34, 0x56, 0x78, 0x90];
    options.p1 = 1;
    options.totalRecords = 2;
    writeRecord(record);
    if (options.callback) {
      options.callback(options);
    }
  };

  io.loadNextRecord = function fakeLoadNextRecord(options) {
    // Unused bytes.
    let record = [0xff, 0xff, 0xff, 0xff, 0xff];
    options.p1++;
    writeRecord(record);
    if (options.callback) {
      options.callback(options);
    }
  };

  let fileId = 0x0000; // Dummy.
  recordHelper.findFreeRecordId(
    fileId,
    function(recordId) {
      do_check_eq(recordId, 2);
      run_next_test();
    }.bind(this),
    function(errorMsg) {
      do_print(errorMsg);
      do_check_true(false);
      run_next_test();
    }.bind(this));
});

/**
 * Verify ICCContactHelper.readICCContacts
 */
add_test(function test_read_icc_contacts() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let record = context.ICCRecordHelper;
  let contactHelper = context.ICCContactHelper;
  let ril = context.RIL;

  function do_test(aSimType, aContactType, aExpectedContact, aEnhancedPhoneBook) {
    ril.appType = aSimType;
    ril._isCdma = (aSimType === CARD_APPTYPE_RUIM);
    ril.iccInfoPrivate.cst = (aEnhancedPhoneBook) ?
                                    [0x0, 0x0C, 0x0, 0x0, 0x0]:
                                    [0x0, 0x00, 0x0, 0x0, 0x0];

    // Override some functions to test.
    contactHelper.getContactFieldRecordId = function(pbr, contact, field, onsuccess, onerror) {
      onsuccess(1);
    };

    record.readPBR = function readPBR(onsuccess, onerror) {
      onsuccess([{adn:{fileId: 0x6f3a}, email: {}, anr0: {}}]);
    };

    record.readADNLike = function readADNLike(fileId, onsuccess, onerror) {
      onsuccess([{recordId: 1, alphaId: "name", number: "111111"}])
    };

    record.readEmail = function readEmail(fileId, fileType, recordNumber, onsuccess, onerror) {
      onsuccess("hello@mail.com");
    };

    record.readANR = function readANR(fileId, fileType, recordNumber, onsuccess, onerror) {
      onsuccess("123456");
    };

    let onsuccess = function onsuccess(contacts) {
      let contact = contacts[0];
      for (let key in contact) {
        do_print("check " + key);
        if (Array.isArray(contact[key])) {
          do_check_eq(contact[key][0], aExpectedContact[key]);
        } else {
          do_check_eq(contact[key], aExpectedContact[key]);
        }
      }
    };

    let onerror = function onerror(errorMsg) {
      do_print("readICCContacts failed: " + errorMsg);
      do_check_true(false);
    };

    contactHelper.readICCContacts(aSimType, aContactType, onsuccess, onerror);
  }

  let expectedContact1 = {
    pbrIndex: 0,
    recordId: 1,
    alphaId:  "name",
    number:   "111111"
  };

  let expectedContact2 = {
    pbrIndex: 0,
    recordId: 1,
    alphaId:  "name",
    number:   "111111",
    email:    "hello@mail.com",
    anr:      "123456"
  };

  // SIM
  do_print("Test read SIM adn contacts");
  do_test(CARD_APPTYPE_SIM, "adn", expectedContact1);

  do_print("Test read SIM fdn contacts");
  do_test(CARD_APPTYPE_SIM, "fdn", expectedContact1);

  // USIM
  do_print("Test read USIM adn contacts");
  do_test(CARD_APPTYPE_USIM, "adn", expectedContact2);

  do_print("Test read USIM fdn contacts");
  do_test(CARD_APPTYPE_USIM, "fdn", expectedContact1);

  // RUIM
  do_print("Test read RUIM adn contacts");
  do_test(CARD_APPTYPE_RUIM, "adn", expectedContact1);

  do_print("Test read RUIM fdn contacts");
  do_test(CARD_APPTYPE_RUIM, "fdn", expectedContact1);

  // RUIM with enhanced phone book
  do_print("Test read RUIM adn contacts with enhanced phone book");
  do_test(CARD_APPTYPE_RUIM, "adn", expectedContact2, true);

  do_print("Test read RUIM fdn contacts with enhanced phone book");
  do_test(CARD_APPTYPE_RUIM, "fdn", expectedContact1, true);

  run_next_test();
});

/**
 * Verify ICCContactHelper.updateICCContact with appType is CARD_APPTYPE_USIM.
 */
add_test(function test_update_icc_contact() {
  const ADN_RECORD_ID   = 100;
  const ADN_SFI         = 1;
  const IAP_FILE_ID     = 0x4f17;
  const EMAIL_FILE_ID   = 0x4f50;
  const EMAIL_RECORD_ID = 20;
  const ANR0_FILE_ID    = 0x4f11;
  const ANR0_RECORD_ID  = 30;

  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let recordHelper = context.ICCRecordHelper;
  let contactHelper = context.ICCContactHelper;
  let ril = context.RIL;

  function do_test(aSimType, aContactType, aContact, aPin2, aFileType, aHaveIapIndex, aEnhancedPhoneBook) {
    ril.appType = aSimType;
    ril._isCdma = (aSimType === CARD_APPTYPE_RUIM);
    ril.iccInfoPrivate.cst = (aEnhancedPhoneBook) ? [0x0, 0x0C, 0x0, 0x0, 0x0]
                                                  : [0x0, 0x00, 0x0, 0x0, 0x0];

    recordHelper.readPBR = function(onsuccess, onerror) {
      if (aFileType === ICC_USIM_TYPE1_TAG) {
        onsuccess([{
          adn:   {fileId: ICC_EF_ADN},
          email: {fileId: EMAIL_FILE_ID,
                  fileType: ICC_USIM_TYPE1_TAG},
          anr0:  {fileId: ANR0_FILE_ID,
                  fileType: ICC_USIM_TYPE1_TAG}
        }]);
      } else if (aFileType === ICC_USIM_TYPE2_TAG) {
        onsuccess([{
          adn:   {fileId: ICC_EF_ADN,
                  sfi: ADN_SFI},
          iap:   {fileId: IAP_FILE_ID},
          email: {fileId: EMAIL_FILE_ID,
                  fileType: ICC_USIM_TYPE2_TAG,
                  indexInIAP: 0},
          anr0:  {fileId: ANR0_FILE_ID,
                  fileType: ICC_USIM_TYPE2_TAG,
                  indexInIAP: 1}
        }]);
      }
    };

    recordHelper.updateADNLike = function(fileId, contact, pin2, onsuccess, onerror) {
      if (aContactType === "fdn") {
        do_check_eq(fileId, ICC_EF_FDN);
      } else if (aContactType === "adn") {
        do_check_eq(fileId, ICC_EF_ADN);
      }
      do_check_eq(pin2, aPin2);
      do_check_eq(contact.alphaId, aContact.alphaId);
      do_check_eq(contact.number, aContact.number);
      onsuccess();
    };

    recordHelper.readIAP = function(fileId, recordNumber, onsuccess, onerror) {
      do_check_eq(fileId, IAP_FILE_ID);
      do_check_eq(recordNumber, ADN_RECORD_ID);
      onsuccess((aHaveIapIndex) ? [EMAIL_RECORD_ID, ANR0_RECORD_ID]
                                : [0xff, 0xff]);
    };

    recordHelper.updateIAP = function(fileId, recordNumber, iap, onsuccess, onerror) {
      do_check_eq(fileId, IAP_FILE_ID);
      do_check_eq(recordNumber, ADN_RECORD_ID);
      onsuccess();
    };

    recordHelper.updateEmail = function(pbr, recordNumber, email, adnRecordId, onsuccess, onerror) {
      do_check_eq(pbr.email.fileId, EMAIL_FILE_ID);
      if (pbr.email.fileType === ICC_USIM_TYPE1_TAG) {
        do_check_eq(recordNumber, ADN_RECORD_ID);
      } else if (pbr.email.fileType === ICC_USIM_TYPE2_TAG) {
        do_check_eq(recordNumber, EMAIL_RECORD_ID);
      }
      do_check_eq(email, aContact.email);
      onsuccess();
    };

    recordHelper.updateANR = function(pbr, recordNumber, number, adnRecordId, onsuccess, onerror) {
      do_check_eq(pbr.anr0.fileId, ANR0_FILE_ID);
      if (pbr.anr0.fileType === ICC_USIM_TYPE1_TAG) {
        do_check_eq(recordNumber, ADN_RECORD_ID);
      } else if (pbr.anr0.fileType === ICC_USIM_TYPE2_TAG) {
        do_check_eq(recordNumber, ANR0_RECORD_ID);
      }
      if (Array.isArray(aContact.anr)) {
        do_check_eq(number, aContact.anr[0]);
      }
      onsuccess();
    };

    recordHelper.findFreeRecordId = function(fileId, onsuccess, onerror) {
      let recordId = 0;
      if (fileId === EMAIL_FILE_ID) {
        recordId = EMAIL_RECORD_ID;
      } else if (fileId === ANR0_FILE_ID) {
        recordId = ANR0_RECORD_ID;
      }
      onsuccess(recordId);
    };

    let isSuccess = false;
    let onsuccess = function onsuccess() {
      do_print("updateICCContact success");
      isSuccess = true;
    };

    let onerror = function onerror(errorMsg) {
      do_print("updateICCContact failed: " + errorMsg);
    };

    contactHelper.updateICCContact(aSimType, aContactType, aContact, aPin2, onsuccess, onerror);
    do_check_true(isSuccess);
  }

  let contacts = [
    {
      pbrIndex: 0,
      recordId: ADN_RECORD_ID,
      alphaId:  "test",
      number:   "123456",
      email:    "test@mail.com",
      anr:      ["+654321"]
    },
    // a contact without email and anr.
    {
      pbrIndex: 0,
      recordId: ADN_RECORD_ID,
      alphaId:  "test2",
      number:   "123456",
    },
    // a contact with email but no anr.
    {
      pbrIndex: 0,
      recordId: ADN_RECORD_ID,
      alphaId:  "test3",
      number:   "123456",
      email:    "test@mail.com"
    },
    // a contact with anr but no email.
    {
      pbrIndex: 0,
      recordId: ADN_RECORD_ID,
      alphaId:  "test4",
      number:   "123456",
      anr:      ["+654321"]
    }];

  for (let i = 0; i < contacts.length; i++) {
    let contact = contacts[i];
    // SIM
    do_print("Test update SIM adn contacts");
    do_test(CARD_APPTYPE_SIM, "adn", contact);

    do_print("Test update SIM fdn contacts");
    do_test(CARD_APPTYPE_SIM, "fdn", contact, "1234");

    // USIM
    do_print("Test update USIM adn contacts");
    do_test(CARD_APPTYPE_USIM, "adn", contact, null, ICC_USIM_TYPE1_TAG);
    do_test(CARD_APPTYPE_USIM, "adn", contact, null, ICC_USIM_TYPE2_TAG, true);
    do_test(CARD_APPTYPE_USIM, "adn", contact, null, ICC_USIM_TYPE2_TAG, false);

    do_print("Test update USIM fdn contacts");
    do_test(CARD_APPTYPE_USIM, "fdn", contact, "1234");

    // RUIM
    do_print("Test update RUIM adn contacts");
    do_test(CARD_APPTYPE_RUIM, "adn", contact);

    do_print("Test update RUIM fdn contacts");
    do_test(CARD_APPTYPE_RUIM, "fdn", contact, "1234");

    // RUIM with enhanced phone book
    do_print("Test update RUIM adn contacts with enhanced phone book");
    do_test(CARD_APPTYPE_RUIM, "adn", contact, null, ICC_USIM_TYPE1_TAG, null, true);
    do_test(CARD_APPTYPE_RUIM, "adn", contact, null, ICC_USIM_TYPE2_TAG, true, true);
    do_test(CARD_APPTYPE_RUIM, "adn", contact, null, ICC_USIM_TYPE2_TAG, false, true);

    do_print("Test update RUIM fdn contacts with enhanced phone book");
    do_test(CARD_APPTYPE_RUIM, "fdn", contact, "1234", null, true);
  }

  run_next_test();
});

/**
 * Verify updateICCContact with removal of anr and email with File Type 1.
 */
add_test(function test_update_icc_contact_with_remove_type1_attr() {
  const ADN_RECORD_ID   = 100;
  const IAP_FILE_ID     = 0x4f17;
  const EMAIL_FILE_ID   = 0x4f50;
  const EMAIL_RECORD_ID = 20;
  const ANR0_FILE_ID    = 0x4f11;
  const ANR0_RECORD_ID  = 30;

  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let recordHelper = context.ICCRecordHelper;
  let contactHelper = context.ICCContactHelper;

  recordHelper.updateADNLike = function(fileId, contact, pin2, onsuccess, onerror) {
    onsuccess();
  };

  let contact = {
    pbrIndex: 0,
    recordId: ADN_RECORD_ID,
    alphaId:  "test2",
    number:   "123456",
  };

  recordHelper.readIAP = function(fileId, recordNumber, onsuccess, onerror) {
    onsuccess([EMAIL_RECORD_ID, ANR0_RECORD_ID]);
  };

  recordHelper.updateEmail = function(pbr, recordNumber, email, adnRecordId, onsuccess, onerror) {
    do_check_true(email == null);
    onsuccess();
  };

  recordHelper.updateANR = function(pbr, recordNumber, number, adnRecordId, onsuccess, onerror) {
    do_check_true(number == null);
    onsuccess();
  };

  function do_test(type) {
    recordHelper.readPBR = function(onsuccess, onerror) {
      if (type == ICC_USIM_TYPE1_TAG) {
        onsuccess([{
          adn:   {fileId: ICC_EF_ADN},
          email: {fileId: EMAIL_FILE_ID,
                  fileType: ICC_USIM_TYPE1_TAG},
          anr0:  {fileId: ANR0_FILE_ID,
                  fileType: ICC_USIM_TYPE1_TAG}}]);
      } else {
        onsuccess([{
          adn:   {fileId: ICC_EF_ADN},
          iap:   {fileId: IAP_FILE_ID},
          email: {fileId: EMAIL_FILE_ID,
                  fileType: ICC_USIM_TYPE2_TAG,
                  indexInIAP: 0},
          anr0:  {fileId: ANR0_FILE_ID,
                  fileType: ICC_USIM_TYPE2_TAG,
                  indexInIAP: 1}}]);
      }
    };

    let successCb = function() {
      do_check_true(true);
    };

    let errorCb = function(errorMsg) {
      do_print(errorMsg);
      do_check_true(false);
    };

    contactHelper.updateICCContact(CARD_APPTYPE_USIM, "adn", contact, null, successCb, errorCb);
  }

  do_test(ICC_USIM_TYPE1_TAG);
  do_test(ICC_USIM_TYPE2_TAG);

  run_next_test();
});

/**
 * Verify ICCContactHelper.findFreeICCContact in SIM
 */
add_test(function test_find_free_icc_contact_sim() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let recordHelper = context.ICCRecordHelper;
  let contactHelper = context.ICCContactHelper;
  // Correct record Id starts with 1, so put a null element at index 0.
  let records = [null];
  const MAX_RECORDS = 3;
  const PBR_INDEX = 0;

  recordHelper.findFreeRecordId = function(fileId, onsuccess, onerror) {
    if (records.length > MAX_RECORDS) {
      onerror("No free record found.");
      return;
    }

    onsuccess(records.length);
  };

  let successCb = function(pbrIndex, recordId) {
    do_check_eq(pbrIndex, PBR_INDEX);
    records[recordId] = {};
  };

  let errorCb = function(errorMsg) {
    do_print(errorMsg);
    do_check_true(false);
  };

  for (let i = 0; i < MAX_RECORDS; i++) {
    contactHelper.findFreeICCContact(CARD_APPTYPE_SIM, "adn", successCb, errorCb);
  }
  // The 1st element, records[0], is null.
  do_check_eq(records.length - 1, MAX_RECORDS);

  // Now the EF is full, so finding a free one should result failure.
  successCb = function(pbrIndex, recordId) {
    do_check_true(false);
  };

  errorCb = function(errorMsg) {
    do_check_true(errorMsg === "No free record found.");
  };
  contactHelper.findFreeICCContact(CARD_APPTYPE_SIM, "adn", successCb, errorCb);

  run_next_test();
});

/**
 * Verify ICCContactHelper.findFreeICCContact in USIM
 */
add_test(function test_find_free_icc_contact_usim() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let recordHelper = context.ICCRecordHelper;
  let contactHelper = context.ICCContactHelper;
  const ADN1_FILE_ID = 0x6f3a;
  const ADN2_FILE_ID = 0x6f3b;
  const MAX_RECORDS = 3;

  // The adn in the first phonebook set has already two records, which means
  // only 1 free record remained.
  let pbrs = [{adn: {fileId: ADN1_FILE_ID, records: [null, {}, {}]}},
              {adn: {fileId: ADN2_FILE_ID, records: [null]}}];

  recordHelper.readPBR = function readPBR(onsuccess, onerror) {
    onsuccess(pbrs);
  };

  recordHelper.findFreeRecordId = function(fileId, onsuccess, onerror) {
    let pbr = (fileId == ADN1_FILE_ID ? pbrs[0]: pbrs[1]);
    if (pbr.adn.records.length > MAX_RECORDS) {
      onerror("No free record found.");
      return;
    }

    onsuccess(pbr.adn.records.length);
  };

  let successCb = function(pbrIndex, recordId) {
    do_check_eq(pbrIndex, 0);
    pbrs[pbrIndex].adn.records[recordId] = {};
  };

  let errorCb = function(errorMsg) {
    do_check_true(false);
  };

  contactHelper.findFreeICCContact(CARD_APPTYPE_USIM, "adn", successCb, errorCb);

  // Now the EF_ADN in the 1st phonebook set is full, so the next free contact
  // will come from the 2nd phonebook set.
  successCb = function(pbrIndex, recordId) {
    do_check_eq(pbrIndex, 1);
    do_check_eq(recordId, 1);
  }
  contactHelper.findFreeICCContact(CARD_APPTYPE_USIM, "adn", successCb, errorCb);

  run_next_test();
});

/**
 * Test error message returned in onerror for readICCContacts.
 */
add_test(function test_error_message_read_icc_contact () {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;

  function do_test(options, expectedErrorMsg) {
    ril.sendChromeMessage = function(message) {
      do_check_eq(message.errorMsg, expectedErrorMsg);
    }
    ril.readICCContacts(options);
  }

  // Error 1, didn't specify correct contactType.
  do_test({}, CONTACT_ERR_REQUEST_NOT_SUPPORTED);

  // Error 2, specifying a non-supported contactType.
  ril.appType = CARD_APPTYPE_USIM;
  do_test({contactType: "sdn"}, CONTACT_ERR_CONTACT_TYPE_NOT_SUPPORTED);

  // Error 3, suppose we update the supported PBR fields in USIM_PBR_FIELDS,
  // but forget to add implemenetations for it.
  USIM_PBR_FIELDS.push("pbc");
  do_test({contactType: "adn"}, CONTACT_ERR_FIELD_NOT_SUPPORTED);

  run_next_test();
});

/**
 * Test error message returned in onerror for updateICCContact.
 */
add_test(function test_error_message_update_icc_contact() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;

  const ICCID = "123456789";
  ril.iccInfo.iccid = ICCID;

  function do_test(options, expectedErrorMsg) {
    ril.sendChromeMessage = function(message) {
      do_check_eq(message.errorMsg, expectedErrorMsg);
    }
    ril.updateICCContact(options);
  }

  // Error 1, didn't specify correct contactType.
  do_test({}, CONTACT_ERR_REQUEST_NOT_SUPPORTED);

  // Error 2, specifying a correct contactType, but without providing 'contact'.
  do_test({contactType: "adn"}, CONTACT_ERR_REQUEST_NOT_SUPPORTED);

  // Error 3, specifying a non-supported contactType.
  ril.appType = CARD_APPTYPE_USIM;
  do_test({contactType: "sdn", contact: {}}, CONTACT_ERR_CONTACT_TYPE_NOT_SUPPORTED);

  // Error 4, without supplying pin2.
  do_test({contactType: "fdn", contact: {contactId: ICCID + "1"}}, GECKO_ERROR_SIM_PIN2);

  // Error 5, No free record found in EF_ADN.
  let record = context.ICCRecordHelper;
  record.readPBR = function(onsuccess, onerror) {
    onsuccess([{adn: {fileId: 0x4f3a}}]);
  };

  let io = context.ICCIOHelper;
  io.loadLinearFixedEF = function(options) {
    options.totalRecords = 1;
    options.p1 = 1;
    options.callback(options);
  };

  do_test({contactType: "adn", contact: {}}, CONTACT_ERR_NO_FREE_RECORD_FOUND);

  // Error 6, ICC IO Error.
  io.loadLinearFixedEF = function(options) {
    ril[REQUEST_SIM_IO](0, {rilRequestError: ERROR_GENERIC_FAILURE});
  };
  do_test({contactType: "adn", contact: {contactId: ICCID + "1"}},
          GECKO_ERROR_GENERIC_FAILURE);

  // Error 7, suppose we update the supported PBR fields in USIM_PBR_FIELDS,
  // but forget to add implemenetations for it.
  USIM_PBR_FIELDS.push("pbc");
  do_test({contactType: "adn", contact: {contactId: ICCID + "1"}},
          CONTACT_ERR_FIELD_NOT_SUPPORTED);

  // Error 8, EF_PBR doesn't exist.
  record.readPBR = function(onsuccess, onerror) {
    onsuccess([]);
  };

  do_test({contactType: "adn", contact: {contactId: ICCID + "1"}},
          CONTACT_ERR_CANNOT_ACCESS_PHONEBOOK);

  run_next_test();
});

add_test(function test_process_icc_io_error() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ioHelper = context.ICCIOHelper;

  function do_test(errorCode, expectedErrorMsg) {
    let called = false;
    function errorCb(errorMsg) {
      called = true;
      do_check_eq(errorMsg, expectedErrorMsg);
    }

    ioHelper.processICCIOError({rilRequestError: errorCode,
                                fileId: 0xffff,
                                command: 0xff,
                                sw1: 0xff,
                                sw2: 0xff,
                                onerror: errorCb});
    do_check_true(called);
  }

  for (let i = 0; i < ERROR_REJECTED_BY_REMOTE + 1; i++) {
    do_test(i, RIL_ERROR_TO_GECKO_ERROR[i]);
  }

  run_next_test();
});

add_test(function test_personalization_state() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;

  context.ICCRecordHelper.readICCID = function fakeReadICCID() {};

  function testPersonalization(isCdma, cardPersoState, geckoCardState) {
    let iccStatus = {
      cardState: CARD_STATE_PRESENT,
      gsmUmtsSubscriptionAppIndex: (!isCdma) ? 0 : -1,
      cdmaSubscriptionAppIndex: (isCdma) ? 0 : -1,
      apps: [
        {
          app_state: CARD_APPSTATE_SUBSCRIPTION_PERSO,
          perso_substate: cardPersoState
        }],
    };

    ril._isCdma = isCdma;
    ril._processICCStatus(iccStatus);
    do_check_eq(ril.cardState, geckoCardState);
  }

  // Test GSM personalization state.
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_NETWORK,
                      GECKO_CARDSTATE_NETWORK_LOCKED);
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_CORPORATE,
                      GECKO_CARDSTATE_CORPORATE_LOCKED);
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_SERVICE_PROVIDER,
                      GECKO_CARDSTATE_SERVICE_PROVIDER_LOCKED);
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_NETWORK_PUK,
                      GECKO_CARDSTATE_NETWORK_PUK_REQUIRED);
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_CORPORATE_PUK,
                      GECKO_CARDSTATE_CORPORATE_PUK_REQUIRED);
  testPersonalization(false, CARD_PERSOSUBSTATE_SIM_SERVICE_PROVIDER_PUK,
                      GECKO_CARDSTATE_SERVICE_PROVIDER_PUK_REQUIRED);
  testPersonalization(false, CARD_PERSOSUBSTATE_READY,
                      GECKO_CARDSTATE_PERSONALIZATION_READY);

  // Test CDMA personalization state.
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_NETWORK1,
                      GECKO_CARDSTATE_NETWORK1_LOCKED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_NETWORK2,
                      GECKO_CARDSTATE_NETWORK2_LOCKED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_HRPD,
                      GECKO_CARDSTATE_HRPD_NETWORK_LOCKED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_CORPORATE,
                      GECKO_CARDSTATE_RUIM_CORPORATE_LOCKED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_SERVICE_PROVIDER,
                      GECKO_CARDSTATE_RUIM_SERVICE_PROVIDER_LOCKED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_RUIM,
                      GECKO_CARDSTATE_RUIM_LOCKED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_NETWORK1_PUK,
                      GECKO_CARDSTATE_NETWORK1_PUK_REQUIRED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_NETWORK2_PUK,
                      GECKO_CARDSTATE_NETWORK2_PUK_REQUIRED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_HRPD_PUK,
                      GECKO_CARDSTATE_HRPD_NETWORK_PUK_REQUIRED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_CORPORATE_PUK,
                      GECKO_CARDSTATE_RUIM_CORPORATE_PUK_REQUIRED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_SERVICE_PROVIDER_PUK,
                      GECKO_CARDSTATE_RUIM_SERVICE_PROVIDER_PUK_REQUIRED);
  testPersonalization(true, CARD_PERSOSUBSTATE_RUIM_RUIM_PUK,
                      GECKO_CARDSTATE_RUIM_PUK_REQUIRED);

  run_next_test();
});

/**
 * Verify SIM app_state in _processICCStatus
 */
add_test(function test_card_app_state() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;

  context.ICCRecordHelper.readICCID = function fakeReadICCID() {};

  function testCardAppState(cardAppState, geckoCardState) {
    let iccStatus = {
      cardState: CARD_STATE_PRESENT,
      gsmUmtsSubscriptionAppIndex: 0,
      apps: [
      {
        app_state: cardAppState
      }],
    };

    ril._processICCStatus(iccStatus);
    do_check_eq(ril.cardState, geckoCardState);
  }

  testCardAppState(CARD_APPSTATE_ILLEGAL,
                   GECKO_CARDSTATE_ILLEGAL);
  testCardAppState(CARD_APPSTATE_PIN,
                   GECKO_CARDSTATE_PIN_REQUIRED);
  testCardAppState(CARD_APPSTATE_PUK,
                   GECKO_CARDSTATE_PUK_REQUIRED);
  testCardAppState(CARD_APPSTATE_READY,
                   GECKO_CARDSTATE_READY);
  testCardAppState(CARD_APPSTATE_UNKNOWN,
                   GECKO_CARDSTATE_UNKNOWN);
  testCardAppState(CARD_APPSTATE_DETECTED,
                   GECKO_CARDSTATE_UNKNOWN);

  run_next_test();
});

/**
 * Verify permanent blocked for ICC.
 */
add_test(function test_icc_permanent_blocked() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;

  context.ICCRecordHelper.readICCID = function fakeReadICCID() {};

  function testPermanentBlocked(pin1_replaced, universalPINState, pin1) {
    let iccStatus = {
      cardState: CARD_STATE_PRESENT,
      gsmUmtsSubscriptionAppIndex: 0,
      universalPINState: universalPINState,
      apps: [
      {
        pin1_replaced: pin1_replaced,
        pin1: pin1
      }]
    };

    ril._processICCStatus(iccStatus);
    do_check_eq(ril.cardState, GECKO_CARDSTATE_PERMANENT_BLOCKED);
  }

  testPermanentBlocked(1,
                       CARD_PINSTATE_ENABLED_PERM_BLOCKED,
                       CARD_PINSTATE_UNKNOWN);
  testPermanentBlocked(1,
                       CARD_PINSTATE_ENABLED_PERM_BLOCKED,
                       CARD_PINSTATE_ENABLED_PERM_BLOCKED);
  testPermanentBlocked(0,
                       CARD_PINSTATE_UNKNOWN,
                       CARD_PINSTATE_ENABLED_PERM_BLOCKED);

  run_next_test();
});

/**
 * Verify iccSetCardLock - Facility Lock.
 */
add_test(function test_set_icc_card_lock_facility_lock() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let aid = "123456789";
  let ril = context.RIL;
  ril.aid = aid;
  ril.v5Legacy = false;
  let buf = context.Buf;

  let GECKO_CARDLOCK_TO_FACILITIY_LOCK = {};
  GECKO_CARDLOCK_TO_FACILITIY_LOCK[GECKO_CARDLOCK_PIN] = ICC_CB_FACILITY_SIM;
  GECKO_CARDLOCK_TO_FACILITIY_LOCK[GECKO_CARDLOCK_FDN] = ICC_CB_FACILITY_FDN;

  let GECKO_CARDLOCK_TO_PASSWORD_TYPE = {};
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_PIN] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_FDN] = "pin2";

  const pin = "1234";
  const pin2 = "4321";
  let GECKO_CARDLOCK_TO_PASSWORD = {};
  GECKO_CARDLOCK_TO_PASSWORD[GECKO_CARDLOCK_PIN] = pin;
  GECKO_CARDLOCK_TO_PASSWORD[GECKO_CARDLOCK_FDN] = pin2;

  const serviceClass = ICC_SERVICE_CLASS_VOICE |
                       ICC_SERVICE_CLASS_DATA  |
                       ICC_SERVICE_CLASS_FAX;

  function do_test(aLock, aPassword, aEnabled) {
    buf.sendParcel = function fakeSendParcel () {
      // Request Type.
      do_check_eq(this.readInt32(), REQUEST_SET_FACILITY_LOCK);

      // Token : we don't care
      this.readInt32();

      let parcel = this.readStringList();
      do_check_eq(parcel.length, 5);
      do_check_eq(parcel[0], GECKO_CARDLOCK_TO_FACILITIY_LOCK[aLock]);
      do_check_eq(parcel[1], aEnabled ? "1" : "0");
      do_check_eq(parcel[2], GECKO_CARDLOCK_TO_PASSWORD[aLock]);
      do_check_eq(parcel[3], serviceClass.toString());
      do_check_eq(parcel[4], aid);
    };

    let lock = {lockType: aLock,
                enabled: aEnabled};
    lock[GECKO_CARDLOCK_TO_PASSWORD_TYPE[aLock]] = aPassword;

    ril.iccSetCardLock(lock);
  }

  do_test(GECKO_CARDLOCK_PIN, pin, true);
  do_test(GECKO_CARDLOCK_PIN, pin, false);
  do_test(GECKO_CARDLOCK_FDN, pin2, true);
  do_test(GECKO_CARDLOCK_FDN, pin2, false);

  run_next_test();
});

/**
 * Verify iccUnlockCardLock.
 */
add_test(function test_unlock_card_lock_corporateLocked() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;
  let buf = context.Buf;
  const pin = "12345678";
  const puk = "12345678";

  let GECKO_CARDLOCK_TO_PASSWORD_TYPE = {};
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_NCK] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_NCK1] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_NCK2] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_HNCK] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_CCK] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_SPCK] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_RCCK] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_RSPCK] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_NCK_PUK] = "puk";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_NCK1_PUK] = "puk";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_NCK2_PUK] = "puk";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_HNCK_PUK] = "puk";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_CCK_PUK] = "puk";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_SPCK_PUK] = "puk";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_RCCK_PUK] = "puk";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_RSPCK_PUK] = "puk";

  function do_test(aLock, aPassword) {
    buf.sendParcel = function fakeSendParcel () {
      // Request Type.
      do_check_eq(this.readInt32(), REQUEST_ENTER_NETWORK_DEPERSONALIZATION_CODE);

      // Token : we don't care
      this.readInt32();

      let lockType = GECKO_PERSO_LOCK_TO_CARD_PERSO_LOCK[aLock];
      // Lock Type
      do_check_eq(this.readInt32(), lockType);

      // Pin/Puk.
      do_check_eq(this.readString(), aPassword);
    };

    let lock = {lockType: aLock};
    lock[GECKO_CARDLOCK_TO_PASSWORD_TYPE[aLock]] = aPassword;
    ril.iccUnlockCardLock(lock);
  }

  do_test(GECKO_CARDLOCK_NCK, pin);
  do_test(GECKO_CARDLOCK_NCK1, pin);
  do_test(GECKO_CARDLOCK_NCK2, pin);
  do_test(GECKO_CARDLOCK_HNCK, pin);
  do_test(GECKO_CARDLOCK_CCK, pin);
  do_test(GECKO_CARDLOCK_SPCK, pin);
  do_test(GECKO_CARDLOCK_RCCK, pin);
  do_test(GECKO_CARDLOCK_RSPCK, pin);
  do_test(GECKO_CARDLOCK_NCK_PUK, puk);
  do_test(GECKO_CARDLOCK_NCK1_PUK, puk);
  do_test(GECKO_CARDLOCK_NCK2_PUK, puk);
  do_test(GECKO_CARDLOCK_HNCK_PUK, puk);
  do_test(GECKO_CARDLOCK_CCK_PUK, puk);
  do_test(GECKO_CARDLOCK_SPCK_PUK, puk);
  do_test(GECKO_CARDLOCK_RCCK_PUK, puk);
  do_test(GECKO_CARDLOCK_RSPCK_PUK, puk);

  run_next_test();
});

/**
 * Verify MCC and MNC parsing
 */
add_test(function test_mcc_mnc_parsing() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.ICCUtilsHelper;

  function do_test(imsi, mncLength, expectedMcc, expectedMnc) {
    let result = helper.parseMccMncFromImsi(imsi, mncLength);

    if (!imsi) {
      do_check_eq(result, null);
      return;
    }

    do_check_eq(result.mcc, expectedMcc);
    do_check_eq(result.mnc, expectedMnc);
  }

  // Test the imsi is null.
  do_test(null, null, null, null);

  // Test MCC is Taiwan
  do_test("466923202422409", 0x02, "466", "92");
  do_test("466923202422409", 0x03, "466", "923");
  do_test("466923202422409", null, "466", "92");

  // Test MCC is US
  do_test("310260542718417", 0x02, "310", "26");
  do_test("310260542718417", 0x03, "310", "260");
  do_test("310260542718417", null, "310", "260");

  run_next_test();
 });

 /**
  * Verify reading EF_AD and parsing MCC/MNC
  */
add_test(function test_reading_ad_and_parsing_mcc_mnc() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let record = context.SimRecordHelper;
  let helper = context.GsmPDUHelper;
  let ril    = context.RIL;
  let buf    = context.Buf;
  let io     = context.ICCIOHelper;

  function do_test(mncLengthInEf, imsi, expectedMcc, expectedMnc) {
    ril.iccInfoPrivate.imsi = imsi;

    io.loadTransparentEF = function fakeLoadTransparentEF(options) {
      let ad = [0x00, 0x00, 0x00];
      if (typeof mncLengthInEf === 'number') {
        ad.push(mncLengthInEf);
      }

      // Write data size
      buf.writeInt32(ad.length * 2);

      // Write data
      for (let i = 0; i < ad.length; i++) {
        helper.writeHexOctet(ad[i]);
      }

      // Write string delimiter
      buf.writeStringDelimiter(ad.length * 2);

      if (options.callback) {
        options.callback(options);
      }
    };

    record.readAD();

    do_check_eq(ril.iccInfo.mcc, expectedMcc);
    do_check_eq(ril.iccInfo.mnc, expectedMnc);
  }

  do_test(undefined, "466923202422409", "466", "92" );
  do_test(0x00,      "466923202422409", "466", "92" );
  do_test(0x01,      "466923202422409", "466", "92" );
  do_test(0x02,      "466923202422409", "466", "92" );
  do_test(0x03,      "466923202422409", "466", "923");
  do_test(0x04,      "466923202422409", "466", "92" );
  do_test(0xff,      "466923202422409", "466", "92" );

  do_test(undefined, "310260542718417", "310", "260");
  do_test(0x00,      "310260542718417", "310", "260");
  do_test(0x01,      "310260542718417", "310", "260");
  do_test(0x02,      "310260542718417", "310", "26" );
  do_test(0x03,      "310260542718417", "310", "260");
  do_test(0x04,      "310260542718417", "310", "260");
  do_test(0xff,      "310260542718417", "310", "260");

  run_next_test();
});

add_test(function test_reading_optional_efs() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let record = context.SimRecordHelper;
  let gsmPdu = context.GsmPDUHelper;
  let ril    = context.RIL;
  let buf    = context.Buf;
  let io     = context.ICCIOHelper;

  function buildSST(supportedEf) {
    let sst = [];
    let len = supportedEf.length;
    for (let i = 0; i < len; i++) {
      let index, bitmask, iccService;
      if (ril.appType === CARD_APPTYPE_SIM) {
        iccService = GECKO_ICC_SERVICES.sim[supportedEf[i]];
        iccService -= 1;
        index = Math.floor(iccService / 4);
        bitmask = 2 << ((iccService % 4) << 1);
      } else if (ril.appType === CARD_APPTYPE_USIM){
        iccService = GECKO_ICC_SERVICES.usim[supportedEf[i]];
        iccService -= 1;
        index = Math.floor(iccService / 8);
        bitmask = 1 << ((iccService % 8) << 0);
      }

      if (sst) {
        sst[index] |= bitmask;
      }
    }
    return sst;
  }

  ril.updateCellBroadcastConfig = function fakeUpdateCellBroadcastConfig() {
    // Ignore updateCellBroadcastConfig after reading SST
  };

  function do_test(sst, supportedEf) {
    // Clone supportedEf to local array for testing
    let testEf = supportedEf.slice(0);

    record.readMSISDN = function fakeReadMSISDN() {
      testEf.splice(testEf.indexOf("MSISDN"), 1);
    };

    record.readMBDN = function fakeReadMBDN() {
      testEf.splice(testEf.indexOf("MDN"), 1);
    };

    record.readMWIS = function fakeReadMWIS() {
      testEf.splice(testEf.indexOf("MWIS"), 1);
    };

    io.loadTransparentEF = function fakeLoadTransparentEF(options) {
      // Write data size
      buf.writeInt32(sst.length * 2);

      // Write data
      for (let i = 0; i < sst.length; i++) {
         gsmPdu.writeHexOctet(sst[i] || 0);
      }

      // Write string delimiter
      buf.writeStringDelimiter(sst.length * 2);

      if (options.callback) {
        options.callback(options);
      }

      if (testEf.length !== 0) {
        do_print("Un-handled EF: " + JSON.stringify(testEf));
        do_check_true(false);
      }
    };

    record.readSST();
  }

  // TODO: Add all necessary optional EFs eventually
  let supportedEf = ["MSISDN", "MDN", "MWIS"];
  ril.appType = CARD_APPTYPE_SIM;
  do_test(buildSST(supportedEf), supportedEf);
  ril.appType = CARD_APPTYPE_USIM;
  do_test(buildSST(supportedEf), supportedEf);

  run_next_test();
});

/**
 * Verify fetchSimRecords.
 */
add_test(function test_fetch_sim_recodes() {
  let worker = newWorker();
  let context = worker.ContextPool._contexts[0];
  let RIL = context.RIL;
  let iccRecord = context.ICCRecordHelper;
  let simRecord = context.SimRecordHelper;

  function testFetchSimRecordes(expectCalled) {
    let ifCalled = [];

    RIL.getIMSI = function() {
      ifCalled.push("getIMSI");
    };

    simRecord.readAD = function() {
      ifCalled.push("readAD");
    };

    simRecord.readSST = function() {
      ifCalled.push("readSST");
    };

    simRecord.fetchSimRecords();

    for (let i = 0; i < expectCalled.length; i++ ) {
      if (ifCalled[i] != expectCalled[i]) {
        do_print(expectCalled[i] + " is not called.");
        do_check_true(false);
      }
    }
  }

  let expectCalled = ["getIMSI", "readAD", "readSST"];
  testFetchSimRecordes(expectCalled);

  run_next_test();
});

add_test(function test_fetch_icc_recodes() {
  let worker = newWorker();
  let context = worker.ContextPool._contexts[0];
  let RIL = context.RIL;
  let iccRecord = context.ICCRecordHelper;
  let simRecord = context.SimRecordHelper;
  let ruimRecord = context.RuimRecordHelper;
  let fetchTag = 0x00;

  simRecord.fetchSimRecords = function() {
    fetchTag = 0x01;
  };

  ruimRecord.fetchRuimRecords = function() {
    fetchTag = 0x02;
  };

  RIL.appType = CARD_APPTYPE_SIM;
  iccRecord.fetchICCRecords();
  do_check_eq(fetchTag, 0x01);

  RIL.appType = CARD_APPTYPE_RUIM;
  iccRecord.fetchICCRecords();
  do_check_eq(fetchTag, 0x02);

  RIL.appType = CARD_APPTYPE_USIM;
  iccRecord.fetchICCRecords();
  do_check_eq(fetchTag, 0x01);

  run_next_test();
});

/**
 * Verify SimRecordHelper.readMWIS
 */
add_test(function test_read_mwis() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let recordHelper = context.SimRecordHelper;
  let buf    = context.Buf;
  let io     = context.ICCIOHelper;
  let mwisData;
  let postedMessage;

  worker.postMessage = function fakePostMessage(message) {
    postedMessage = message;
  };

  io.loadLinearFixedEF = function fakeLoadLinearFixedEF(options) {
    if (mwisData) {
      // Write data size
      buf.writeInt32(mwisData.length * 2);

      // Write MWIS
      for (let i = 0; i < mwisData.length; i++) {
        helper.writeHexOctet(mwisData[i]);
      }

      // Write string delimiter
      buf.writeStringDelimiter(mwisData.length * 2);

      options.recordSize = mwisData.length;
      if (options.callback) {
        options.callback(options);
      }
    } else {
      do_print("mwisData[] is not set.");
    }
  };

  function buildMwisData(isActive, msgCount) {
    if (msgCount < 0 || msgCount === GECKO_VOICEMAIL_MESSAGE_COUNT_UNKNOWN) {
      msgCount = 0;
    } else if (msgCount > 255) {
      msgCount = 255;
    }

    mwisData =  [ (isActive) ? 0x01 : 0x00,
                  msgCount,
                  0xFF, 0xFF, 0xFF ];
  }

  function do_test(isActive, msgCount) {
    buildMwisData(isActive, msgCount);
    recordHelper.readMWIS();

    do_check_eq("iccmwis", postedMessage.rilMessageType);
    do_check_eq(isActive, postedMessage.mwi.active);
    do_check_eq((isActive) ? msgCount : 0, postedMessage.mwi.msgCount);
  }

  do_test(true, GECKO_VOICEMAIL_MESSAGE_COUNT_UNKNOWN);
  do_test(true, 1);
  do_test(true, 255);

  do_test(false, 0);
  do_test(false, 255); // Test the corner case when mwi is disable with incorrect msgCount.

  run_next_test();
});

/**
 * Verify SimRecordHelper.updateMWIS
 */
add_test(function test_update_mwis() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let ril = context.RIL;
  ril.appType = CARD_APPTYPE_USIM;
  ril.iccInfoPrivate.mwis = [0x00, 0x00, 0x00, 0x00, 0x00];
  let recordHelper = context.SimRecordHelper;
  let buf = context.Buf;
  let ioHelper = context.ICCIOHelper;
  let recordSize = ril.iccInfoPrivate.mwis.length;
  let recordNum = 1;

  ioHelper.updateLinearFixedEF = function(options) {
    options.pathId = context.ICCFileHelper.getEFPath(options.fileId);
    options.command = ICC_COMMAND_UPDATE_RECORD;
    options.p1 = options.recordNumber;
    options.p2 = READ_RECORD_ABSOLUTE_MODE;
    options.p3 = recordSize;
    ril.iccIO(options);
  };

  function do_test(isActive, count) {
    let mwis = ril.iccInfoPrivate.mwis;
    let isUpdated = false;

    function buildMwisData() {
      let result = mwis.slice(0);
      result[0] = isActive? (mwis[0] | 0x01) : (mwis[0] & 0xFE);
      result[1] = (count === GECKO_VOICEMAIL_MESSAGE_COUNT_UNKNOWN) ? 0 : count;

      return result;
    }

    buf.sendParcel = function() {
      isUpdated = true;

      // Request Type.
      do_check_eq(this.readInt32(), REQUEST_SIM_IO);

      // Token : we don't care
      this.readInt32();

      // command.
      do_check_eq(this.readInt32(), ICC_COMMAND_UPDATE_RECORD);

      // fileId.
      do_check_eq(this.readInt32(), ICC_EF_MWIS);

      // pathId.
      do_check_eq(this.readString(),
                  EF_PATH_MF_SIM + ((ril.appType === CARD_APPTYPE_USIM) ? EF_PATH_ADF_USIM : EF_PATH_DF_GSM));

      // p1.
      do_check_eq(this.readInt32(), recordNum);

      // p2.
      do_check_eq(this.readInt32(), READ_RECORD_ABSOLUTE_MODE);

      // p3.
      do_check_eq(this.readInt32(), recordSize);

      // data.
      let strLen = this.readInt32();
      do_check_eq(recordSize * 2, strLen);
      let expectedMwis = buildMwisData();
      for (let i = 0; i < recordSize; i++) {
        do_check_eq(expectedMwis[i], pduHelper.readHexOctet());
      }
      this.readStringDelimiter(strLen);

      // pin2.
      do_check_eq(this.readString(), null);

      if (!ril.v5Legacy) {
        // AID. Ignore because it's from modem.
        this.readInt32();
      }
    };

    do_check_false(isUpdated);

    recordHelper.updateMWIS({ active: isActive,
                              msgCount: count });

    do_check_true((ril.iccInfoPrivate.mwis) ? isUpdated : !isUpdated);
  }

  do_test(true, GECKO_VOICEMAIL_MESSAGE_COUNT_UNKNOWN);
  do_test(true, 1);
  do_test(true, 255);

  do_test(false, 0);

  // Test if Path ID is correct for SIM.
  ril.appType = CARD_APPTYPE_SIM;
  do_test(false, 0);

  // Test if loadLinearFixedEF() is not invoked in updateMWIS() when
  // EF_MWIS is not loaded/available.
  delete ril.iccInfoPrivate.mwis;
  do_test(false, 0);

  run_next_test();
});

/**
 * Verify the call flow of receiving Class 2 SMS stored in SIM:
 * 1. UNSOLICITED_RESPONSE_NEW_SMS_ON_SIM.
 * 2. SimRecordHelper.readSMS().
 * 3. sendChromeMessage() with rilMessageType == "sms-received".
 */
add_test(function test_read_new_sms_on_sim() {
  // Instead of reusing newUint8Worker defined in this file,
  // we define our own worker to fake the methods in WorkerBuffer dynamically.
  function newSmsOnSimWorkerHelper() {
    let _postedMessage;
    let _worker = newWorker({
      postRILMessage: function(data) {
      },
      postMessage: function(message) {
        _postedMessage = message;
      }
    });

    _worker.debug = do_print;

    return {
      get postedMessage() {
        return _postedMessage;
      },
      get worker() {
        return _worker;
      },
      fakeWokerBuffer: function() {
        let context = _worker.ContextPool._contexts[0];
        let index = 0; // index for read
        let buf = [];
        context.Buf.writeUint8 = function(value) {
          buf.push(value);
        };
        context.Buf.readUint8 = function() {
          return buf[index++];
        };
        context.Buf.seekIncoming = function(offset) {
          index += offset;
        };
        context.Buf.getReadAvailable = function() {
          return buf.length - index;
        };
      }
    };
  }

  let workerHelper = newSmsOnSimWorkerHelper();
  let worker = workerHelper.worker;
  let context = worker.ContextPool._contexts[0];

  context.ICCIOHelper.loadLinearFixedEF = function fakeLoadLinearFixedEF(options) {
      // SimStatus: Unread, SMSC:+0123456789, Sender: +9876543210, Text: How are you?
      let SimSmsPduHex = "0306911032547698040A9189674523010000208062917314080CC8F71D14969741F977FD07"
                       // In 4.2.25 EF_SMS Short Messages of 3GPP TS 31.102:
                       // 1. Record length == 176 bytes.
                       // 2. Any bytes in the record following the TPDU shall be filled with 'FF'.
                       + "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                       + "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                       + "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                       + "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF";

      workerHelper.fakeWokerBuffer();

      context.Buf.writeString(SimSmsPduHex);

      options.recordSize = 176; // Record length is fixed to 176 bytes.
      if (options.callback) {
        options.callback(options);
      }
  };

  function newSmsOnSimParcel() {
    let data = new Uint8Array(4 + 4); // Int32List with 1 element.
    let offset = 0;

    function writeInt(value) {
      data[offset++] = value & 0xFF;
      data[offset++] = (value >>  8) & 0xFF;
      data[offset++] = (value >> 16) & 0xFF;
      data[offset++] = (value >> 24) & 0xFF;
    }

    writeInt(1); // Length of Int32List
    writeInt(1); // RecordNum = 1.

    return newIncomingParcel(-1,
                             RESPONSE_TYPE_UNSOLICITED,
                             UNSOLICITED_RESPONSE_NEW_SMS_ON_SIM,
                             data);
  }

  function do_test() {
    worker.onRILMessage(0, newSmsOnSimParcel());

    let postedMessage = workerHelper.postedMessage;

    do_check_eq("sms-received", postedMessage.rilMessageType);
    do_check_eq("+0123456789", postedMessage.SMSC);
    do_check_eq("+9876543210", postedMessage.sender);
    do_check_eq("How are you?", postedMessage.body);
  }

  do_test();

  run_next_test();
});

// Test ICC_COMMAND_GET_RESPONSE with FCP template format.
/**
 * Verify transparent structure with FCP template format.
 */
add_test(function test_fcp_template_for_transparent_structure() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;

  let tag_test = [
    0x62,
    0x22,
    0x82, 0x02, 0x41, 0x21,
    0x83, 0x02, 0x2F, 0xE2,
    0xA5, 0x09, 0xC1, 0x04, 0x40, 0x0F, 0xF5, 0x55, 0x92, 0x01, 0x00,
    0x8A, 0x01, 0x05,
    0x8B, 0x03, 0x2F, 0x06, 0x0B,
    0x80, 0x02, 0x00, 0x0A,
    0x88, 0x01, 0x10];

  for (let i = 0; i < tag_test.length; i++) {
    pduHelper.writeHexOctet(tag_test[i]);
  }

  let berTlv = berHelper.decode(tag_test.length);
  let iter = Iterator(berTlv.value);
  let tlv = berHelper.searchForNextTag(BER_FCP_FILE_DESCRIPTOR_TAG, iter);
  do_check_eq(tlv.value.fileStructure, UICC_EF_STRUCTURE[EF_TYPE_TRANSPARENT]);

  tlv = berHelper.searchForNextTag(BER_FCP_FILE_IDENTIFIER_TAG, iter);
  do_check_eq(tlv.value.fileId, 0x2FE2);

  tlv = berHelper.searchForNextTag(BER_FCP_FILE_SIZE_DATA_TAG, iter);
  do_check_eq(tlv.value.fileSizeData, 0x0A);

  run_next_test();
});

/**
 * Verify linear fixed structure with FCP template format.
 */
add_test(function test_fcp_template_for_linear_fixed_structure() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let pduHelper = context.GsmPDUHelper;
  let berHelper = context.BerTlvHelper;

  let tag_test = [
    0x62,
    0x1E,
    0x82, 0x05, 0x42, 0x21, 0x00, 0x1A, 0x01,
    0x83, 0x02, 0x6F, 0x40,
    0xA5, 0x03, 0x92, 0x01, 0x00,
    0x8A, 0x01, 0x07,
    0x8B, 0x03, 0x6F, 0x06, 0x02,
    0x80, 0x02, 0x00, 0x1A,
    0x88, 0x00];

  for (let i = 0; i < tag_test.length; i++) {
    pduHelper.writeHexOctet(tag_test[i]);
  }

  let berTlv = berHelper.decode(tag_test.length);
  let iter = Iterator(berTlv.value);
  let tlv = berHelper.searchForNextTag(BER_FCP_FILE_DESCRIPTOR_TAG, iter);
  do_check_eq(tlv.value.fileStructure, UICC_EF_STRUCTURE[EF_TYPE_LINEAR_FIXED]);
  do_check_eq(tlv.value.recordLength, 0x1A);
  do_check_eq(tlv.value.numOfRecords, 0x01);

  tlv = berHelper.searchForNextTag(BER_FCP_FILE_IDENTIFIER_TAG, iter);
  do_check_eq(tlv.value.fileId, 0x6F40);

  tlv = berHelper.searchForNextTag(BER_FCP_FILE_SIZE_DATA_TAG, iter);
  do_check_eq(tlv.value.fileSizeData, 0x1A);

  run_next_test();
});

add_test(function test_icc_io_get_response_for_transparent_structure() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;
  let iccioHelper = context.ICCIOHelper;
  let pduHelper = context.GsmPDUHelper;

  let responseArray = [
    // SIM response.
    [0x00, 0x00, 0x00, 0x0A, 0x2F, 0xE2, 0x04, 0x00, 0x0A, 0xA0, 0xAA, 0x00,
     0x02, 0x00, 0x00],
    // USIM response.
    [0x62, 0x22, 0x82, 0x02, 0x41, 0x21, 0x83, 0x02, 0x2F, 0xE2, 0xA5, 0x09,
     0xC1, 0x04, 0x40, 0x0F, 0xF5, 0x55, 0x92, 0x01, 0x00, 0x8A, 0x01, 0x05,
     0x8B, 0x03, 0x2F, 0x06, 0x0B, 0x80, 0x02, 0x00, 0x0A, 0x88, 0x01, 0x10]
  ];

  for (let i = 0; i < responseArray.length; i++) {
    let strLen = responseArray[i].length * 2;
    buf.writeInt32(strLen);
    for (let j = 0; j < responseArray[i].length; j++) {
      pduHelper.writeHexOctet(responseArray[i][j]);
    }
    buf.writeStringDelimiter(strLen);

    let options = {fileId: ICC_EF_ICCID,
                   type: EF_TYPE_TRANSPARENT};
    iccioHelper.processICCIOGetResponse(options);

    do_check_eq(options.fileSize, 0x0A);
  }

  run_next_test();
});

add_test(function test_icc_io_get_response_for_linear_fixed_structure() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;
  let iccioHelper = context.ICCIOHelper;
  let pduHelper = context.GsmPDUHelper;

  let responseArray = [
    // SIM response.
    [0x00, 0x00, 0x00, 0x1A, 0x6F, 0x40, 0x04, 0x00, 0x11, 0xA0, 0xAA, 0x00,
     0x02, 0x01, 0x1A],
    // USIM response.
    [0x62, 0x1E, 0x82, 0x05, 0x42, 0x21, 0x00, 0x1A, 0x01, 0x83, 0x02, 0x6F,
     0x40, 0xA5, 0x03, 0x92, 0x01, 0x00, 0x8A, 0x01, 0x07, 0x8B, 0x03, 0x6F,
     0x06, 0x02, 0x80, 0x02, 0x00, 0x1A, 0x88, 0x00]
  ];

  for (let i = 0; i < responseArray.length; i++) {
    let strLen = responseArray[i].length * 2;
    buf.writeInt32(strLen);
    for (let j = 0; j < responseArray[i].length; j++) {
      pduHelper.writeHexOctet(responseArray[i][j]);
    }
    buf.writeStringDelimiter(strLen);

    let options = {fileId: ICC_EF_MSISDN,
                   type: EF_TYPE_LINEAR_FIXED};
    iccioHelper.processICCIOGetResponse(options);

    do_check_eq(options.fileSize, 0x1A);
    do_check_eq(options.recordSize, 0x1A);
    do_check_eq(options.totalRecords, 0x01);
  }

  run_next_test();
});

/**
 * Verify reading EF_ICCID.
 */
add_test(function test_handling_iccid() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let record = context.ICCRecordHelper;
  let helper = context.GsmPDUHelper;
  let ril = context.RIL;
  let buf = context.Buf;
  let io = context.ICCIOHelper;

  ril.reportStkServiceIsRunning = function fakeReportStkServiceIsRunning() {
  };

  function do_test(rawICCID, expectedICCID) {
    io.loadTransparentEF = function fakeLoadTransparentEF(options) {
      // Write data size
      buf.writeInt32(rawICCID.length);

      // Write data
      for (let i = 0; i < rawICCID.length; i += 2) {
        helper.writeHexOctet(parseInt(rawICCID.substr(i, 2), 16));
      }

      // Write string delimiter
      buf.writeStringDelimiter(rawICCID.length);

      if (options.callback) {
        options.callback(options);
      }
    };

    record.readICCID();

    do_check_eq(ril.iccInfo.iccid, expectedICCID);
  }

  // Invalid char at high nibbile + low nibbile contains 0xF.
  do_test("9868002E90909F001519", "89860020909");
  // Invalid char at low nibbile.
  do_test("986800E2909090001519", "8986002090909005191");
  // Valid ICCID.
  do_test("98101430121181157002", "89014103211118510720");

  run_next_test();
});
