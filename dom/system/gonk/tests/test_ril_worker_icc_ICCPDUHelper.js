/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
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
  equal(iccHelper.readICCUCS2String(0x80, (2 * text.length) + ffLen), text);

  // 0x81
  let array = [0x08, 0xd2, 0x4d, 0x6f, 0x7a, 0x69, 0x6c, 0x6c, 0x61, 0xca,
               0xff, 0xff];
  let len = array.length;
  for (let i = 0; i < len; i++) {
    helper.writeHexOctet(array[i]);
  }
  equal(iccHelper.readICCUCS2String(0x81, len), "Mozilla\u694a");

  // 0x82
  let array2 = [0x08, 0x69, 0x00, 0x4d, 0x6f, 0x7a, 0x69, 0x6c, 0x6c, 0x61,
                0xca, 0xff, 0xff];
  let len2 = array2.length;
  for (let i = 0; i < len2; i++) {
    helper.writeHexOctet(array2[i]);
  }
  equal(iccHelper.readICCUCS2String(0x82, len2), "Mozilla\u694a");

  run_next_test();
});

/**
 * Verify ICCPDUHelper#writeICCUCS2String()
 */
add_test(function test_write_icc_ucs2_string() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let iccHelper = context.ICCPDUHelper;
  let alphaLen = 18;
  let test_data = [
    {
      encode: 0x80,
      // string only contain one character.
      data: "\u82b3"
    }, {
      encode: 0x80,
      // 2 UCS2 character not located in the same half-page.
      data: "Fire \u82b3\u8233"
    }, {
      encode: 0x80,
      // 2 UCS2 character not located in the same half-page.
      data: "\u694a\u704a"
    }, {
      encode: 0x81,
      // 2 UCS2 character within same half-page.
      data: "Fire \u6901\u697f"
    }, {
      encode: 0x81,
      // 2 UCS2 character within same half-page.
      data: "Fire \u6980\u69ff"
    }, {
      encode: 0x82,
      // 2 UCS2 character within same half-page, but bit 8 is different.
      data: "Fire \u0514\u0593"
    }, {
      encode: 0x82,
      // 2 UCS2 character over 0x81 can encode range.
      data: "Fire \u8000\u8001"
    }, {
      encode: 0x82,
      // 2 UCS2 character over 0x81 can encode range.
      data: "Fire \ufffd\ufffe"
    }];

  for (let i = 0; i < test_data.length; i++) {
    let test = test_data[i];
    let writtenStr = iccHelper.writeICCUCS2String(alphaLen, test.data);
    equal(writtenStr, test.data);
    equal(helper.readHexOctet(), test.encode);
    equal(iccHelper.readICCUCS2String(test.encode, alphaLen - 1), test.data);
  }

  // This string use 0x80 encoded and the maximum capacity is 17 octets.
  // Each alphabet takes 2 octets, thus the first 8 alphabets can be written.
  let str = "Mozilla \u82b3\u8233 On Fire";
  let writtenStr = iccHelper.writeICCUCS2String(alphaLen, str);
  equal(writtenStr, str.substring(0, 8));
  equal(helper.readHexOctet(), 0x80);
  equal(iccHelper.readICCUCS2String(0x80, alphaLen - 1), str.substring(0, 8));

  // This string use 0x81 encoded and the maximum capacity is 15 octets.
  // Each alphabet takes 1 octets, thus the first 15 alphabets can be written.
  str = "Mozilla \u6901\u697f On Fire";
  writtenStr = iccHelper.writeICCUCS2String(alphaLen, str);
  equal(writtenStr, str.substring(0, 15));
  equal(helper.readHexOctet(), 0x81);
  equal(iccHelper.readICCUCS2String(0x81, alphaLen - 1), str.substring(0, 15));

  // This string use 0x82 encoded and the maximum capacity is 14 octets.
  // Each alphabet takes 1 octets, thus the first 14 alphabets can be written.
  str = "Mozilla \u0514\u0593 On Fire";
  writtenStr = iccHelper.writeICCUCS2String(alphaLen, str);
  equal(writtenStr, str.substring(0, 14));
  equal(helper.readHexOctet(), 0x82);
  equal(iccHelper.readICCUCS2String(0x82, alphaLen - 1), str.substring(0, 14));

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

  helper.readSwappedNibbleExtendedBcdString = function(len) {
    return str.substring(0, len);
  };

  for (let i = 0; i < str.length; i++) {
    equal(str.substring(0, i - 1), // -1 for the TON
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

  equal(iccHelper.read8BitUnpackedToString(PDU_NL_EXTENDED_ESCAPE),
              langTable.substring(0, PDU_NL_EXTENDED_ESCAPE));
  equal(iccHelper.read8BitUnpackedToString(2), " ");
  equal(iccHelper.read8BitUnpackedToString(langTable.length -
                                              PDU_NL_EXTENDED_ESCAPE - 1 + ffLen),
              langTable.substring(PDU_NL_EXTENDED_ESCAPE + 1));

  // Test 2: Read GSM extended alphabets.
  for (let i = 0; i < langShiftTable.length; i++) {
    helper.writeHexOctet(PDU_NL_EXTENDED_ESCAPE);
    helper.writeHexOctet(i);
  }

  // Read string before RESERVED_CONTROL.
  equal(iccHelper.read8BitUnpackedToString(PDU_NL_RESERVED_CONTROL  * 2),
              langShiftTable.substring(0, PDU_NL_RESERVED_CONTROL));
  // ESCAPE + RESERVED_CONTROL will become ' '.
  equal(iccHelper.read8BitUnpackedToString(2), " ");
  // Read string between RESERVED_CONTROL and EXTENDED_ESCAPE.
  equal(iccHelper.read8BitUnpackedToString(
                (PDU_NL_EXTENDED_ESCAPE - PDU_NL_RESERVED_CONTROL - 1)  * 2),
              langShiftTable.substring(PDU_NL_RESERVED_CONTROL + 1,
                                       PDU_NL_EXTENDED_ESCAPE));
  // ESCAPE + ESCAPE will become ' '.
  equal(iccHelper.read8BitUnpackedToString(2), " ");
  // Read remaining string.
  equal(iccHelper.read8BitUnpackedToString(
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
  let writtenStr = iccHelper.writeStringTo8BitUnpacked(langTable.length + ffLen, langTable);
  equal(writtenStr, langTable);

  for (let i = 0; i < langTable.length; i++) {
    equal(helper.readHexOctet(), i);
  }

  for (let i = 0; i < ffLen; i++) {
    equal(helper.readHexOctet(), 0xff);
  }

  // Test 2, write GSM extended alphabets.
  str = "\u000c\u20ac";
  writtenStr = iccHelper.writeStringTo8BitUnpacked(4, str);
  equal(writtenStr, str);
  equal(iccHelper.read8BitUnpackedToString(4), str);

  // Test 3, write GSM and GSM extended alphabets.
  // \u000c, \u20ac are from gsm extended alphabets.
  // \u00a3 is from gsm alphabet.
  str = "\u000c\u20ac\u00a3";

  // 2 octets * 2 = 4 octets for 2 gsm extended alphabets,
  // 1 octet for 1 gsm alphabet,
  // 2 octes for trailing 0xff.
  // "Totally 7 octets are to be written."
  writtenStr = iccHelper.writeStringTo8BitUnpacked(7, str);
  equal(writtenStr, str);
  equal(iccHelper.read8BitUnpackedToString(7), str);

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
  let writtenStr = iccHelper.writeStringTo8BitUnpacked(3, langTable.substring(0, 4));
  equal(writtenStr, langTable.substring(0, 3));
  helper.writeHexOctet(0xff); // dummy octet.
  for (let i = 0; i < 3; i++) {
    equal(helper.readHexOctet(), i);
  }
  ok(helper.readHexOctet() != 4);

  // \u000c is GSM extended alphabet, 2 octets.
  // \u00a3 is GSM alphabet, 1 octet.
  let str = "\u000c\u00a3";
  writtenStr = iccHelper.writeStringTo8BitUnpacked(3, str);
  equal(writtenStr, str.substring(0, 2));
  equal(iccHelper.read8BitUnpackedToString(3), str);

  str = "\u00a3\u000c";
  writtenStr = iccHelper.writeStringTo8BitUnpacked(3, str);
  equal(writtenStr, str.substring(0, 2));
  equal(iccHelper.read8BitUnpackedToString(3), str);

  // 2 GSM extended alphabets cost 4 octets, but maximum is 3, so only the 1st
  // alphabet can be written.
  str = "\u000c\u000c";
  writtenStr = iccHelper.writeStringTo8BitUnpacked(3, str);
  helper.writeHexOctet(0xff); // dummy octet.
  equal(writtenStr, str.substring(0, 1));
  equal(iccHelper.read8BitUnpackedToString(4), str.substring(0, 1));

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
  equal(iccHelper.readAlphaIdentifier(1 + (2 * text.length) + ffLen), text);

  // UCS2: 0x81
  let array = [0x81, 0x08, 0xd2, 0x4d, 0x6f, 0x7a, 0x69, 0x6c, 0x6c, 0x61, 0xca, 0xff, 0xff];
  for (let i = 0; i < array.length; i++) {
    helper.writeHexOctet(array[i]);
  }
  equal(iccHelper.readAlphaIdentifier(array.length), "Mozilla\u694a");

  // UCS2: 0x82
  let array2 = [0x82, 0x08, 0x69, 0x00, 0x4d, 0x6f, 0x7a, 0x69, 0x6c, 0x6c, 0x61, 0xca, 0xff, 0xff];
  for (let i = 0; i < array2.length; i++) {
    helper.writeHexOctet(array2[i]);
  }
  equal(iccHelper.readAlphaIdentifier(array2.length), "Mozilla\u694a");

  // GSM 8 Bit Unpacked
  const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
  for (let i = 0; i < PDU_NL_EXTENDED_ESCAPE; i++) {
    helper.writeHexOctet(i);
  }
  equal(iccHelper.readAlphaIdentifier(PDU_NL_EXTENDED_ESCAPE),
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
  let writenAlphaId = iccHelper.writeAlphaIdentifier(10, null);
  equal(writenAlphaId, null);
  equal(iccHelper.readAlphaIdentifier(10), "");

  // GSM 8 bit
  let str = "Mozilla";
  writenAlphaId = iccHelper.writeAlphaIdentifier(str.length + ffLen, str);
  equal(writenAlphaId , str);
  equal(iccHelper.readAlphaIdentifier(str.length + ffLen), str);

  // UCS2
  str = "Mozilla\u8000";
  writenAlphaId = iccHelper.writeAlphaIdentifier(str.length * 2 + ffLen, str);
  equal(writenAlphaId , str);
  // * 2 for each character will be encoded to UCS2 alphabets.
  equal(iccHelper.readAlphaIdentifier(str.length * 2 + ffLen), str);

  // Test with maximum octets written.
  // 1 coding scheme (0x80) and 1 UCS2 character, total 3 octets.
  str = "\u694a";
  writenAlphaId = iccHelper.writeAlphaIdentifier(3, str);
  equal(writenAlphaId , str);
  equal(iccHelper.readAlphaIdentifier(3), str);

  // 1 coding scheme (0x80) and 2 UCS2 characters, total 5 octets.
  // numOctets is limited to 4, so only 1 UCS2 character can be written.
  str = "\u694a\u69ca";
  writenAlphaId = iccHelper.writeAlphaIdentifier(4, str);
  helper.writeHexOctet(0xff); // dummy octet.
  equal(writenAlphaId , str.substring(0, 1));
  equal(iccHelper.readAlphaIdentifier(5), str.substring(0, 1));

  // Write 0 octet.
  writenAlphaId = iccHelper.writeAlphaIdentifier(0, "1");
  helper.writeHexOctet(0xff); // dummy octet.
  equal(writenAlphaId, null);
  equal(iccHelper.readAlphaIdentifier(1), "");

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
      equal(contactR, null);
    } else {
      equal(contactR.alphaId, contact.alphaId);
      equal(contactR.number, contact.number);
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

  let writtenContact = helper.writeAlphaIdDiallingNumber(recordSize,
                                                         contactW.alphaId,
                                                         contactW.number, 0xff);

  let contactR = helper.readAlphaIdDiallingNumber(recordSize);
  equal(writtenContact.alphaId, contactR.alphaId);
  equal(writtenContact.number, contactR.number);
  equal(0xff, contactR.extRecordNumber);

  // Write a contact with alphaId encoded in UCS2 and number has '+'.
  let contactUCS2 = {
    alphaId: "火狐",
    number: "+1234567890"
  };

  writtenContact = helper.writeAlphaIdDiallingNumber(recordSize,
                                                     contactUCS2.alphaId,
                                                     contactUCS2.number, 0xff);
  contactR = helper.readAlphaIdDiallingNumber(recordSize);
  equal(writtenContact.alphaId, contactR.alphaId);
  equal(writtenContact.number, contactR.number);
  equal(0xff, contactR.extRecordNumber);

  // Write a null contact (Removal).
  writtenContact = helper.writeAlphaIdDiallingNumber(recordSize);
  contactR = helper.readAlphaIdDiallingNumber(recordSize);
  equal(contactR, null);
  equal(writtenContact.alphaId, null);
  equal(writtenContact.number, null);

  // Write a longer alphaId/dialling number
  // Dialling Number : Maximum 20 digits(10 octets).
  // Alpha Identifier: 32(recordSize) - 14 (10 octets for Dialling Number, 1
  //                   octet for TON/NPI, 1 for number length octet, and 2 for
  //                   Ext) = Maximum 18 octets.
  let longContact = {
    alphaId: "AAAAAAAAABBBBBBBBBCCCCCCCCC",
    number: "123456789012345678901234567890",
  };

  writtenContact = helper.writeAlphaIdDiallingNumber(recordSize,
                                                     longContact.alphaId,
                                                     longContact.number, 0xff);
  contactR = helper.readAlphaIdDiallingNumber(recordSize);
  equal(writtenContact.alphaId, contactR.alphaId);
  equal(writtenContact.number, contactR.number);
  equal(0xff, contactR.extRecordNumber);

  // Add '+' to number and test again.
  longContact.number = "+123456789012345678901234567890";
  writtenContact = helper.writeAlphaIdDiallingNumber(recordSize,
                                                     longContact.alphaId,
                                                     longContact.number, 0xff);
  contactR = helper.readAlphaIdDiallingNumber(recordSize);
  equal(writtenContact.alphaId, contactR.alphaId);
  equal(writtenContact.number, contactR.number);
  equal(0xff, contactR.extRecordNumber);

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
  equal(helper.readDiallingNumber(len), number);

  // without +
  number = "987654";
  len = 4;
  helper.writeDiallingNumber(number);
  equal(helper.readDiallingNumber(len), number);

  number = "9876543";
  len = 5;
  helper.writeDiallingNumber(number);
  equal(helper.readDiallingNumber(len), number);

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

  let numbers = [
    {
      number: "123456789",
      expectedNumber: "123456789"
    },
    {
      number: "",
      expectedNumber: ""
    },
    // Invalid length of BCD number/SSC contents
    {
      number: "12345678901234567890123",
      expectedNumber: ""
    },
  ];

  // To avoid obtaining wrong buffer content.
  context.Buf.seekIncoming = function(offset) {
  };

  function do_test(aNumber, aExpectedNumber) {
    iccHelper.readDiallingNumber = function(numLen) {
      return aNumber.substring(0, numLen);
    };

    if (aNumber) {
      helper.writeHexOctet(aNumber.length + 1);
    } else {
      helper.writeHexOctet(0xff);
    }

    equal(iccHelper.readNumberWithLength(), aExpectedNumber);
  }

  for (let i = 0; i < numbers.length; i++) {
    do_test(numbers[i].number, numbers[i].expectedNumber);
  }

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
    let writeNumber = iccHelper.writeNumberWithLength(number);
    equal(writeNumber, expectedNumber);
    let numLen = helper.readHexOctet();
    equal(expectedNumber, iccHelper.readDiallingNumber(numLen));
    for (let i = 0; i < (ADN_MAX_BCD_NUMBER_BYTES - numLen); i++) {
      equal(0xff, helper.readHexOctet());
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

  // over maximum 20 digits should be truncated.
  test("012345678901234567890123456789", "01234567890123456789");

  // null
  iccHelper.writeNumberWithLength(null);
  for (let i = 0; i < (ADN_MAX_BCD_NUMBER_BYTES + 1); i++) {
    equal(0xff, helper.readHexOctet());
  }

  run_next_test();
});
