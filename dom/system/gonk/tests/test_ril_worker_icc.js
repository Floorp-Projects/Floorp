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

  worker.Buf.writeUint8 = function (value) {
    buf.push(value);
  };

  worker.Buf.readUint8 = function () {
    return buf[index++];
  };

  worker.Buf.seekIncoming = function (offset) {
    index += offset;
  };

  worker.debug = do_print;

  return worker;
}
/**
 * Verify GsmPDUHelper#readICCUCS2String()
 */
add_test(function test_read_icc_ucs2_string() {
  let worker = newUint8Worker();
  let helper = worker.GsmPDUHelper;

  // 0x80
  let text = "TEST";
  helper.writeUCS2String(text);
  // Also write two unused octets.
  let ffLen = 2;
  for (let i = 0; i < ffLen; i++) {
    helper.writeHexOctet(0xff);
  }
  do_check_eq(helper.readICCUCS2String(0x80, (2 * text.length) + ffLen), text);

  // 0x81
  let array = [0x08, 0xd2, 0x4d, 0x6f, 0x7a, 0x69, 0x6c, 0x6c, 0x61, 0xca,
               0xff, 0xff];
  let len = array.length;
  for (let i = 0; i < len; i++) {
    helper.writeHexOctet(array[i]);
  }
  do_check_eq(helper.readICCUCS2String(0x81, len), "Mozilla\u694a");

  // 0x82
  let array2 = [0x08, 0x69, 0x00, 0x4d, 0x6f, 0x7a, 0x69, 0x6c, 0x6c, 0x61,
                0xca, 0xff, 0xff];
  let len2 = array2.length;
  for (let i = 0; i < len2; i++) {
    helper.writeHexOctet(array2[i]);
  }
  do_check_eq(helper.readICCUCS2String(0x82, len2), "Mozilla\u694a");

  run_next_test();
});

/**
 * Verify GsmPDUHelper#read8BitUnpackedToString
 */
add_test(function test_read_8bit_unpacked_to_string() {
  let worker = newUint8Worker();
  let helper = worker.GsmPDUHelper;
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

  do_check_eq(helper.read8BitUnpackedToString(PDU_NL_EXTENDED_ESCAPE),
              langTable.substring(0, PDU_NL_EXTENDED_ESCAPE));
  do_check_eq(helper.read8BitUnpackedToString(2), " ");
  do_check_eq(helper.read8BitUnpackedToString(langTable.length -
                                              PDU_NL_EXTENDED_ESCAPE - 1 + ffLen),
              langTable.substring(PDU_NL_EXTENDED_ESCAPE + 1));

  // Test 2: Read GSM extended alphabets.
  for (let i = 0; i < langShiftTable.length; i++) {
    helper.writeHexOctet(PDU_NL_EXTENDED_ESCAPE);
    helper.writeHexOctet(i);
  }

  // Read string before RESERVED_CONTROL.
  do_check_eq(helper.read8BitUnpackedToString(PDU_NL_RESERVED_CONTROL  * 2),
              langShiftTable.substring(0, PDU_NL_RESERVED_CONTROL));
  // ESCAPE + RESERVED_CONTROL will become ' '.
  do_check_eq(helper.read8BitUnpackedToString(2), " ");
  // Read string between RESERVED_CONTROL and EXTENDED_ESCAPE.
  do_check_eq(helper.read8BitUnpackedToString(
                (PDU_NL_EXTENDED_ESCAPE - PDU_NL_RESERVED_CONTROL - 1)  * 2),
              langShiftTable.substring(PDU_NL_RESERVED_CONTROL + 1,
                                       PDU_NL_EXTENDED_ESCAPE));
  // ESCAPE + ESCAPE will become ' '.
  do_check_eq(helper.read8BitUnpackedToString(2), " ");
  // Read remaining string.
  do_check_eq(helper.read8BitUnpackedToString(
                (langShiftTable.length - PDU_NL_EXTENDED_ESCAPE - 1)  * 2),
              langShiftTable.substring(PDU_NL_EXTENDED_ESCAPE + 1));

  run_next_test();
});

/**
 * Verify GsmPDUHelper#writeStringTo8BitUnpacked.
 *
 * Test writing GSM 8 bit alphabets.
 */
add_test(function test_write_string_to_8bit_unpacked() {
  let worker = newUint8Worker();
  let helper = worker.GsmPDUHelper;
  const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
  const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
  // Length of trailing 0xff.
  let ffLen = 2;
  let str;

  // Test 1, write GSM alphabets.
  helper.writeStringTo8BitUnpacked(langTable.length + ffLen, langTable);

  for (let i = 0; i < langTable.length; i++) {
    do_check_eq(helper.readHexOctet(), i);
  }

  for (let i = 0; i < ffLen; i++) {
    do_check_eq(helper.readHexOctet(), 0xff);
  }

  // Test 2, write GSM extended alphabets.
  str = "\u000c\u20ac";
  helper.writeStringTo8BitUnpacked(4, str);

  do_check_eq(helper.read8BitUnpackedToString(4), str);

  // Test 3, write GSM and GSM extended alphabets.
  // \u000c, \u20ac are from gsm extended alphabets.
  // \u00a3 is from gsm alphabet.
  str = "\u000c\u20ac\u00a3";

  // 2 octets * 2 = 4 octets for 2 gsm extended alphabets,
  // 1 octet for 1 gsm alphabet,
  // 2 octes for trailing 0xff.
  // "Totally 7 octets are to be written."
  helper.writeStringTo8BitUnpacked(7, str);

  do_check_eq(helper.read8BitUnpackedToString(7), str);

  run_next_test();
});

/**
 * Verify GsmPDUHelper#writeStringTo8BitUnpacked with maximum octets written.
 */
add_test(function test_write_string_to_8bit_unpacked_with_max_octets_written() {
  let worker = newUint8Worker();
  let helper = worker.GsmPDUHelper;
  const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
  const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];

  // The maximum of the number of octets that can be written is 3.
  // Only 3 characters shall be written even the length of the string is 4.
  helper.writeStringTo8BitUnpacked(3, langTable.substring(0, 4));
  helper.writeHexOctet(0xff); // dummy octet.
  for (let i = 0; i < 3; i++) {
    do_check_eq(helper.readHexOctet(), i);
  }
  do_check_false(helper.readHexOctet() == 4);

  // \u000c is GSM extended alphabet, 2 octets.
  // \u00a3 is GSM alphabet, 1 octet.
  let str = "\u000c\u00a3";
  helper.writeStringTo8BitUnpacked(3, str);
  do_check_eq(helper.read8BitUnpackedToString(3), str);

  str = "\u00a3\u000c";
  helper.writeStringTo8BitUnpacked(3, str);
  do_check_eq(helper.read8BitUnpackedToString(3), str);

  // 2 GSM extended alphabets cost 4 octets, but maximum is 3, so only the 1st
  // alphabet can be written.
  str = "\u000c\u000c";
  helper.writeStringTo8BitUnpacked(3, str);
  helper.writeHexOctet(0xff); // dummy octet.
  do_check_eq(helper.read8BitUnpackedToString(4), str.substring(0, 1));

  run_next_test();
});

/**
 * Verify GsmPDUHelper.writeAlphaIdentifier
 */
add_test(function test_write_alpha_identifier() {
  let worker = newUint8Worker();
  let helper = worker.GsmPDUHelper;
  // Length of trailing 0xff.
  let ffLen = 2;

  // Removal
  helper.writeAlphaIdentifier(10, null);
  do_check_eq(helper.readAlphaIdentifier(10), "");

  // GSM 8 bit
  let str = "Mozilla";
  helper.writeAlphaIdentifier(str.length + ffLen, str);
  do_check_eq(helper.readAlphaIdentifier(str.length + ffLen), str);

  // UCS2
  str = "Mozilla\u694a";
  helper.writeAlphaIdentifier(str.length * 2 + ffLen, str);
  // * 2 for each character will be encoded to UCS2 alphabets.
  do_check_eq(helper.readAlphaIdentifier(str.length * 2 + ffLen), str);

  // Test with maximum octets written.
  // 1 coding scheme (0x80) and 1 UCS2 character, total 3 octets.
  str = "\u694a";
  helper.writeAlphaIdentifier(3, str);
  do_check_eq(helper.readAlphaIdentifier(3), str);

  // 1 coding scheme (0x80) and 2 UCS2 characters, total 5 octets.
  // numOctets is limited to 4, so only 1 UCS2 character can be written.
  str = "\u694a\u694a";
  helper.writeAlphaIdentifier(4, str);
  helper.writeHexOctet(0xff); // dummy octet.
  do_check_eq(helper.readAlphaIdentifier(5), str.substring(0, 1));

  // Write 0 octet.
  helper.writeAlphaIdentifier(0, "1");
  helper.writeHexOctet(0xff); // dummy octet.
  do_check_eq(helper.readAlphaIdentifier(1), "");

  run_next_test();
});

/**
 * Verify GsmPDUHelper.writeAlphaIdDiallingNumber
 */
add_test(function test_write_alpha_id_dialling_number() {
  let worker = newUint8Worker();
  let helper = worker.GsmPDUHelper;
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
 * Verify GsmPDUHelper.writeDiallingNumber
 */
add_test(function test_write_dialling_number() {
  let worker = newUint8Worker();
  let helper = worker.GsmPDUHelper;

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
 * Verify GsmPDUHelper.writeTimestamp
 */
add_test(function test_write_timestamp() {
  let worker = newUint8Worker();
  let helper = worker.GsmPDUHelper;

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
  let helper = worker.GsmPDUHelper;

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
  let ICCUtilsHelper = worker.ICCUtilsHelper;

  function test_table(sst, geckoService, simEnabled, usimEnabled) {
    worker.RIL.iccInfo.sst = sst;
    worker.RIL.appType = CARD_APPTYPE_SIM;
    do_check_eq(ICCUtilsHelper.isICCServiceAvailable(geckoService), simEnabled);
    worker.RIL.appType = CARD_APPTYPE_USIM;
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
  let ICCUtilsHelper = worker.ICCUtilsHelper;
  const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
  const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];

  do_check_eq(ICCUtilsHelper.isGsm8BitAlphabet(langTable), true);
  do_check_eq(ICCUtilsHelper.isGsm8BitAlphabet(langShiftTable), true);
  do_check_eq(ICCUtilsHelper.isGsm8BitAlphabet("\uaaaa"), false);

  run_next_test();
});

/**
 * Verify RIL.sendStkTerminalProfile
 */
add_test(function test_send_stk_terminal_profile() {
  let worker = newUint8Worker();
  let ril = worker.RIL;
  let buf = worker.Buf;

  ril.sendStkTerminalProfile(STK_SUPPORTED_TERMINAL_PROFILE);

  buf.seekIncoming(8);
  let profile = buf.readString();
  for (let i = 0; i < STK_SUPPORTED_TERMINAL_PROFILE.length; i++) {
    do_check_eq(parseInt(profile.substring(2 * i, 2 * i + 2), 16),
                STK_SUPPORTED_TERMINAL_PROFILE[i]);
  }

  run_next_test();
});

/**
 * Verify ComprehensionTlvHelper.writeLocationInfoTlv
 */
add_test(function test_write_location_info_tlv() {
  let worker = newUint8Worker();
  let pduHelper = worker.GsmPDUHelper;
  let tlvHelper = worker.ComprehensionTlvHelper;

  // Test with 2-digit mnc, and gsmCellId obtained from UMTS network.
  let loc = {
    mcc: 466,
    mnc: 92,
    gsmLocationAreaCode : 10291,
    gsmCellId: 19072823
  };
  tlvHelper.writeLocationInfoTlv(loc);

  let tag = pduHelper.readHexOctet();
  do_check_eq(tag, COMPREHENSIONTLV_TAG_LOCATION_INFO |
                   COMPREHENSIONTLV_FLAG_CR);

  let length = pduHelper.readHexOctet();
  do_check_eq(length, 9);

  let mcc_mnc = pduHelper.readSwappedNibbleBcdString(3);
  do_check_eq(mcc_mnc, "46692");

  let lac = (pduHelper.readHexOctet() << 8) | pduHelper.readHexOctet();
  do_check_eq(lac, 10291);

  let cellId = (pduHelper.readHexOctet() << 24) |
               (pduHelper.readHexOctet() << 16) |
               (pduHelper.readHexOctet() << 8)  |
               (pduHelper.readHexOctet());
  do_check_eq(cellId, 19072823);

  // Test with 1-digit mnc, and gsmCellId obtained from GSM network.
  loc = {
    mcc: 466,
    mnc: 2,
    gsmLocationAreaCode : 10291,
    gsmCellId: 65534
  };
  tlvHelper.writeLocationInfoTlv(loc);

  tag = pduHelper.readHexOctet();
  do_check_eq(tag, COMPREHENSIONTLV_TAG_LOCATION_INFO |
                   COMPREHENSIONTLV_FLAG_CR);

  length = pduHelper.readHexOctet();
  do_check_eq(length, 7);

  mcc_mnc = pduHelper.readSwappedNibbleBcdString(3);
  do_check_eq(mcc_mnc, "46602");

  lac = (pduHelper.readHexOctet() << 8) | pduHelper.readHexOctet();
  do_check_eq(lac, 10291);

  cellId = (pduHelper.readHexOctet() << 8) | (pduHelper.readHexOctet());
  do_check_eq(cellId, 65534);

  // Test with 3-digit mnc, and gsmCellId obtained from GSM network.
  loc = {
    mcc: 466,
    mnc: 222,
    gsmLocationAreaCode : 10291,
    gsmCellId: 65534
  };
  tlvHelper.writeLocationInfoTlv(loc);

  tag = pduHelper.readHexOctet();
  do_check_eq(tag, COMPREHENSIONTLV_TAG_LOCATION_INFO |
                   COMPREHENSIONTLV_FLAG_CR);

  length = pduHelper.readHexOctet();
  do_check_eq(length, 7);

  mcc_mnc = pduHelper.readSwappedNibbleBcdString(3);
  do_check_eq(mcc_mnc, "466222");

  lac = (pduHelper.readHexOctet() << 8) | pduHelper.readHexOctet();
  do_check_eq(lac, 10291);

  cellId = (pduHelper.readHexOctet() << 8) | (pduHelper.readHexOctet());
  do_check_eq(cellId, 65534);

  run_next_test();
});

/**
 * Verify ComprehensionTlvHelper.writeErrorNumber
 */
add_test(function test_write_disconnecting_cause() {
  let worker = newUint8Worker();
  let pduHelper = worker.GsmPDUHelper;
  let tlvHelper = worker.ComprehensionTlvHelper;

  tlvHelper.writeCauseTlv(RIL_ERROR_TO_GECKO_ERROR[ERROR_GENERIC_FAILURE]);
  let tag = pduHelper.readHexOctet();
  do_check_eq(tag, COMPREHENSIONTLV_TAG_CAUSE | COMPREHENSIONTLV_FLAG_CR);
  let len = pduHelper.readHexOctet();
  do_check_eq(len, 2);  // We have one cause.
  let standard = pduHelper.readHexOctet();
  do_check_eq(standard, 0x60);
  let cause = pduHelper.readHexOctet();
  do_check_eq(cause, 0x80 | ERROR_GENERIC_FAILURE);

  run_next_test();
});

/**
 * Verify ComprehensionTlvHelper.getSizeOfLengthOctets
 */
add_test(function test_get_size_of_length_octets() {
  let worker = newUint8Worker();
  let tlvHelper = worker.ComprehensionTlvHelper;

  let length = 0x70;
  do_check_eq(tlvHelper.getSizeOfLengthOctets(length), 1);

  length = 0x80;
  do_check_eq(tlvHelper.getSizeOfLengthOctets(length), 2);

  length = 0x180;
  do_check_eq(tlvHelper.getSizeOfLengthOctets(length), 3);

  length = 0x18000;
  do_check_eq(tlvHelper.getSizeOfLengthOctets(length), 4);

  run_next_test();
});

/**
 * Verify ComprehensionTlvHelper.writeLength
 */
add_test(function test_write_length() {
  let worker = newUint8Worker();
  let pduHelper = worker.GsmPDUHelper;
  let tlvHelper = worker.ComprehensionTlvHelper;

  let length = 0x70;
  tlvHelper.writeLength(length);
  do_check_eq(pduHelper.readHexOctet(), length);

  length = 0x80;
  tlvHelper.writeLength(length);
  do_check_eq(pduHelper.readHexOctet(), 0x81);
  do_check_eq(pduHelper.readHexOctet(), length);

  length = 0x180;
  tlvHelper.writeLength(length);
  do_check_eq(pduHelper.readHexOctet(), 0x82);
  do_check_eq(pduHelper.readHexOctet(), (length >> 8) & 0xff);
  do_check_eq(pduHelper.readHexOctet(), length & 0xff);

  length = 0x18000;
  tlvHelper.writeLength(length);
  do_check_eq(pduHelper.readHexOctet(), 0x83);
  do_check_eq(pduHelper.readHexOctet(), (length >> 16) & 0xff);
  do_check_eq(pduHelper.readHexOctet(), (length >> 8) & 0xff);
  do_check_eq(pduHelper.readHexOctet(), length & 0xff);

  run_next_test();
});

/**
 * Verify Proactive Command : Refresh
 */
add_test(function test_stk_proactive_command_refresh() {
  let worker = newUint8Worker();
  let pduHelper = worker.GsmPDUHelper;
  let berHelper = worker.BerTlvHelper;
  let stkHelper = worker.StkProactiveCmdHelper;

  let refresh_1 = [
    0xD0,
    0x10,
    0x81, 0x03, 0x01, 0x01, 0x01,
    0x82, 0x02, 0x81, 0x82,
    0x92, 0x05, 0x01, 0x3F, 0x00, 0x2F, 0xE2];

  for (let i = 0; i < refresh_1.length; i++) {
    pduHelper.writeHexOctet(refresh_1[i]);
  }

  let berTlv = berHelper.decode(refresh_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, 0x01);
  do_check_eq(tlv.value.commandQualifier, 0x01);

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_FILE_LIST, ctlvs);
  do_check_eq(tlv.value.fileList, "3F002FE2");

  run_next_test();
});

/**
 * Verify Proactive Command : Play Tone
 */
add_test(function test_stk_proactive_command_play_tone() {
  let worker = newUint8Worker();
  let pduHelper = worker.GsmPDUHelper;
  let berHelper = worker.BerTlvHelper;
  let stkHelper = worker.StkProactiveCmdHelper;

  let tone_1 = [
    0xD0,
    0x1B,
    0x81, 0x03, 0x01, 0x20, 0x00,
    0x82, 0x02, 0x81, 0x03,
    0x85, 0x09, 0x44, 0x69, 0x61, 0x6C, 0x20, 0x54, 0x6F, 0x6E, 0x65,
    0x8E, 0x01, 0x01,
    0x84, 0x02, 0x01, 0x05];

  for (let i = 0; i < tone_1.length; i++) {
    pduHelper.writeHexOctet(tone_1[i]);
  }

  let berTlv = berHelper.decode(tone_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, 0x20);
  do_check_eq(tlv.value.commandQualifier, 0x00);

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_ALPHA_ID, ctlvs);
  do_check_eq(tlv.value.identifier, "Dial Tone");

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_TONE, ctlvs);
  do_check_eq(tlv.value.tone, STK_TONE_TYPE_DIAL_TONE);

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_DURATION, ctlvs);
  do_check_eq(tlv.value.timeUnit, STK_TIME_UNIT_SECOND);
  do_check_eq(tlv.value.timeInterval, 5);

  run_next_test();
});

/**
 * Verify Proactive Command : Poll Interval
 */
add_test(function test_stk_proactive_command_poll_interval() {
  let worker = newUint8Worker();
  let pduHelper = worker.GsmPDUHelper;
  let berHelper = worker.BerTlvHelper;
  let stkHelper = worker.StkProactiveCmdHelper;

  let poll_1 = [
    0xD0,
    0x0D,
    0x81, 0x03, 0x01, 0x03, 0x00,
    0x82, 0x02, 0x81, 0x82,
    0x84, 0x02, 0x01, 0x14];

  for (let i = 0; i < poll_1.length; i++) {
    pduHelper.writeHexOctet(poll_1[i]);
  }

  let berTlv = berHelper.decode(poll_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, 0x03);
  do_check_eq(tlv.value.commandQualifier, 0x00);

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_DURATION, ctlvs);
  do_check_eq(tlv.value.timeUnit, STK_TIME_UNIT_SECOND);
  do_check_eq(tlv.value.timeInterval, 0x14);

  run_next_test();
});

/**
 * Verify Proactive Command: Display Text
 */
add_test(function test_read_septets_to_string() {
  let worker = newUint8Worker();
  let pduHelper = worker.GsmPDUHelper;
  let berHelper = worker.BerTlvHelper;
  let stkHelper = worker.StkProactiveCmdHelper;

  let display_text_1 = [
    0xd0,
    0x28,
    0x81, 0x03, 0x01, 0x21, 0x80,
    0x82, 0x02, 0x81, 0x02,
    0x0d, 0x1d, 0x00, 0xd3, 0x30, 0x9b, 0xfc, 0x06, 0xc9, 0x5c, 0x30, 0x1a,
    0xa8, 0xe8, 0x02, 0x59, 0xc3, 0xec, 0x34, 0xb9, 0xac, 0x07, 0xc9, 0x60,
    0x2f, 0x58, 0xed, 0x15, 0x9b, 0xb9, 0x40,
  ];

  for (let i = 0; i < display_text_1.length; i++) {
    pduHelper.writeHexOctet(display_text_1[i]);
  }

  let berTlv = berHelper.decode(display_text_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_TEXT_STRING, ctlvs);
  do_check_eq(tlv.value.textString, "Saldo 2.04 E. Validez 20/05/13. ");

  run_next_test();
});

add_test(function test_stk_proactive_command_event_list() {
  let worker = newUint8Worker();
  let pduHelper = worker.GsmPDUHelper;
  let berHelper = worker.BerTlvHelper;
  let stkHelper = worker.StkProactiveCmdHelper;

  let event_1 = [
    0xD0,
    0x0F,
    0x81, 0x03, 0x01, 0x05, 0x00,
    0x82, 0x02, 0x81, 0x82,
    0x99, 0x04, 0x00, 0x01, 0x02, 0x03];

  for (let i = 0; i < event_1.length; i++) {
    pduHelper.writeHexOctet(event_1[i]);
  }

  let berTlv = berHelper.decode(event_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, 0x05);
  do_check_eq(tlv.value.commandQualifier, 0x00);

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_EVENT_LIST, ctlvs);
  do_check_eq(Array.isArray(tlv.value.eventList), true);
  for (let i = 0; i < tlv.value.eventList.length; i++) {
    do_check_eq(tlv.value.eventList[i], i);
  }

  run_next_test();
});

add_test(function test_spn_display_condition() {
  let worker = newWorker({
    postRILMessage: function fakePostRILMessage(data) {
      // Do nothing
    },
    postMessage: function fakePostMessage(message) {
      // Do nothing
    }
  });
  let RIL = worker.RIL;
  let ICCUtilsHelper = worker.ICCUtilsHelper;

  // Test updateDisplayCondition runs before any of SIM file is ready.
  do_check_eq(ICCUtilsHelper.updateDisplayCondition(), true);
  do_check_eq(RIL.iccInfo.isDisplayNetworkNameRequired, true);
  do_check_eq(RIL.iccInfo.isDisplaySpnRequired, false);

  // Test with value.
  function testDisplayCondition(iccDisplayCondition,
                                iccMcc, iccMnc, plmnMcc, plmnMnc,
                                expectedIsDisplayNetworkNameRequired,
                                expectedIsDisplaySPNRequired,
                                callback) {
    RIL.iccInfoPrivate.SPN = {
      spnDisplayCondition: iccDisplayCondition
    };
    RIL.iccInfo = {
      mcc: iccMcc,
      mnc: iccMnc
    };
    RIL.operator = {
      mcc: plmnMcc,
      mnc: plmnMnc
    };

    do_check_eq(ICCUtilsHelper.updateDisplayCondition(), true);
    do_check_eq(RIL.iccInfo.isDisplayNetworkNameRequired, expectedIsDisplayNetworkNameRequired);
    do_check_eq(RIL.iccInfo.isDisplaySpnRequired, expectedIsDisplaySPNRequired);
    do_timeout(0, callback);
  };

  function testDisplayConditions(func, caseArray, oncomplete) {
    (function do_call(index) {
      let next = index < (caseArray.length - 1) ? do_call.bind(null, index + 1) : oncomplete;
      caseArray[index].push(next);
      func.apply(null, caseArray[index]);
    })(0);
  }

  testDisplayConditions(testDisplayCondition, [
    [1, 123, 456, 123, 456, true, true],
    [0, 123, 456, 123, 456, false, true],
    [2, 123, 456, 123, 457, false, false],
    [0, 123, 456, 123, 457, false, true],
  ], run_next_test);
});

/**
 * Verify Proactive Command : More Time
 */
add_test(function test_stk_proactive_command_more_time() {
  let worker = newUint8Worker();
  let pduHelper = worker.GsmPDUHelper;
  let berHelper = worker.BerTlvHelper;
  let stkHelper = worker.StkProactiveCmdHelper;

  let more_time_1 = [
    0xD0,
    0x09,
    0x81, 0x03, 0x01, 0x02, 0x00,
    0x82, 0x02, 0x81, 0x82];

  for(let i = 0 ; i < more_time_1.length; i++) {
    pduHelper.writeHexOctet(more_time_1[i]);
  }

  let berTlv = berHelper.decode(more_time_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_MORE_TIME);
  do_check_eq(tlv.value.commandQualifier, 0x00);

  run_next_test();
});

add_test(function read_network_name() {
  let worker = newUint8Worker();
  let helper = worker.GsmPDUHelper;
  let buf = worker.Buf;

  // Returning length of byte.
  function writeNetworkName(isUCS2, requireCi, name) {
    let codingOctet = 0x80;
    let len;
    if (requireCi) {
      codingOctet |= 0x08;
    }

    if (isUCS2) {
      codingOctet |= 0x10;
      len = name.length * 2;
    } else {
      let spare = (8 - (name.length * 7) % 8) % 8;
      codingOctet |= spare;
      len = Math.ceil(name.length * 7 / 8);
    }
    helper.writeHexOctet(codingOctet);

    if (isUCS2) {
      helper.writeUCS2String(name);
    } else {
      helper.writeStringAsSeptets(name, 0, 0, 0);
    }

    return len + 1; // codingOctet.
  }

  function testNetworkName(isUCS2, requireCi, name) {
    let len = writeNetworkName(isUCS2, requireCi, name);
    do_check_eq(helper.readNetworkName(len), name);
  }

  testNetworkName( true,  true, "Test Network Name1");
  testNetworkName( true, false, "Test Network Name2");
  testNetworkName(false,  true, "Test Network Name3");
  testNetworkName(false, false, "Test Network Name4");

  run_next_test();
});

add_test(function test_update_network_name() {
  let RIL = newWorker({
    postRILMessage: function fakePostRILMessage(data) {
      // Do nothing
    },
    postMessage: function fakePostMessage(message) {
      // Do nothing
    }
  }).RIL;

  function testNetworkNameIsNull(operatorMcc, operatorMnc) {
    RIL.operator.mcc = operatorMcc;
    RIL.operator.mnc = operatorMnc;
    do_check_eq(RIL.updateNetworkName(), null);
  }

  function testNetworkName(operatorMcc, operatorMnc,
                            expectedLongName, expectedShortName) {
    RIL.operator.mcc = operatorMcc;
    RIL.operator.mnc = operatorMnc;
    let result = RIL.updateNetworkName();

    do_check_eq(result[0], expectedLongName);
    do_check_eq(result[1], expectedShortName);
  }

  // Before EF_OPL and EF_PNN have been loaded.
  do_check_eq(RIL.updateNetworkName(), null);

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
  testNetworkNameIsNull(123, 457);

  // EF_OPL isn't available and current is in HPLMN,
  // the first record of PNN should be returned.
  testNetworkName(123, 456, "PNN1Long", "PNN1Short");

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
      "mcc": 123,
      "mnc": 457,
      "lacTacStart": 0,
      "lacTacEnd": 0x0010,
      "pnnRecordId": 3
    },
    {
      "mcc": 123,
      "mnc": 457,
      "lacTacStart": 0,
      "lacTacEnd": 0x1010,
      "pnnRecordId": 2
    }
  ];

  // Both EF_PNN and EF_OPL are presented, and current PLMN is HPLMN,
  testNetworkName(123, 456, "PNN4Long", "PNN4Short");

  // Current PLMN is not HPLMN, and according to LAC, we should get
  // the second PNN record.
  testNetworkName(123, 457, "PNN2Long", "PNN2Short");

  run_next_test();
});

/**
 * Verify Proactive Command : Timer Management
 */
add_test(function test_stk_proactive_command_timer_management() {
  let worker = newUint8Worker();
  let pduHelper = worker.GsmPDUHelper;
  let berHelper = worker.BerTlvHelper;
  let stkHelper = worker.StkProactiveCmdHelper;

  // Timer Management - Start
  let timer_management_1 = [
    0xD0,
    0x11,
    0x81, 0x03, 0x01, 0x27, 0x00,
    0x82, 0x02, 0x81, 0x82,
    0xA4, 0x01, 0x01,
    0xA5, 0x03, 0x10, 0x20, 0x30
  ];

  for(let i = 0 ; i < timer_management_1.length; i++) {
    pduHelper.writeHexOctet(timer_management_1[i]);
  }

  let berTlv = berHelper.decode(timer_management_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_TIMER_MANAGEMENT);
  do_check_eq(tlv.value.commandQualifier, STK_TIMER_START);

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_TIMER_IDENTIFIER, ctlvs);
  do_check_eq(tlv.value.timerId, 0x01);

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_TIMER_VALUE, ctlvs);
  do_check_eq(tlv.value.timerValue, (0x01 * 60 * 60) + (0x02 * 60) + 0x03);

  // Timer Management - Deactivate
  let timer_management_2 = [
    0xD0,
    0x0C,
    0x81, 0x03, 0x01, 0x27, 0x01,
    0x82, 0x02, 0x81, 0x82,
    0xA4, 0x01, 0x01
  ];

  for(let i = 0 ; i < timer_management_2.length; i++) {
    pduHelper.writeHexOctet(timer_management_2[i]);
  }

  berTlv = berHelper.decode(timer_management_2.length);
  ctlvs = berTlv.value;
  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_TIMER_MANAGEMENT);
  do_check_eq(tlv.value.commandQualifier, STK_TIMER_DEACTIVATE);

  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_TIMER_IDENTIFIER, ctlvs);
  do_check_eq(tlv.value.timerId, 0x01);

  run_next_test();
});

/**
 * Verify Proactive Command : Provide Local Information
 */
add_test(function test_stk_proactive_command_provide_local_information() {
  let worker = newUint8Worker();
  let pduHelper = worker.GsmPDUHelper;
  let berHelper = worker.BerTlvHelper;
  let stkHelper = worker.StkProactiveCmdHelper;

  // Verify IMEI
  let local_info_1 = [
    0xD0,
    0x09,
    0x81, 0x03, 0x01, 0x26, 0x01,
    0x82, 0x02, 0x81, 0x82];

  for (let i = 0; i < local_info_1.length; i++) {
    pduHelper.writeHexOctet(local_info_1[i]);
  }

  let berTlv = berHelper.decode(local_info_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_PROVIDE_LOCAL_INFO);
  do_check_eq(tlv.value.commandQualifier, STK_LOCAL_INFO_IMEI);

  // Verify Date and Time Zone
  let local_info_2 = [
    0xD0,
    0x09,
    0x81, 0x03, 0x01, 0x26, 0x03,
    0x82, 0x02, 0x81, 0x82];

  for (let i = 0; i < local_info_2.length; i++) {
    pduHelper.writeHexOctet(local_info_2[i]);
  }

  berTlv = berHelper.decode(local_info_2.length);
  ctlvs = berTlv.value;
  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_PROVIDE_LOCAL_INFO);
  do_check_eq(tlv.value.commandQualifier, STK_LOCAL_INFO_DATE_TIME_ZONE);

  run_next_test();
});

add_test(function test_path_id_for_spid_and_spn() {
  let worker = newWorker({
    postRILMessage: function fakePostRILMessage(data) {
      // Do nothing
    },
    postMessage: function fakePostMessage(message) {
      // Do nothing
    }});
  let RIL = worker.RIL;
  let ICCFileHelper = worker.ICCFileHelper;

  // Test SIM
  RIL.iccStatus = {
    gsmUmtsSubscriptionAppIndex: 0,
    apps: [
      {
        app_type: CARD_APPTYPE_SIM
      }, {
        app_type: CARD_APPTYPE_USIM
      }
    ]
  }
  do_check_eq(ICCFileHelper.getEFPath(ICC_EF_SPDI),
              EF_PATH_MF_SIM + EF_PATH_DF_GSM);
  do_check_eq(ICCFileHelper.getEFPath(ICC_EF_SPN),
              EF_PATH_MF_SIM + EF_PATH_DF_GSM);

  // Test USIM
  RIL.iccStatus.gsmUmtsSubscriptionAppIndex = 1;
  do_check_eq(ICCFileHelper.getEFPath(ICC_EF_SPDI),
              EF_PATH_MF_SIM + EF_PATH_ADF_USIM);
  do_check_eq(ICCFileHelper.getEFPath(ICC_EF_SPDI),
              EF_PATH_MF_SIM + EF_PATH_ADF_USIM);
  run_next_test();
});
