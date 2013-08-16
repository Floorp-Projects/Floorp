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

function newUint8SupportOutgoingIndexWorker() {
  let worker = newWorker();
  let index = 4;          // index for read
  let buf = [0, 0, 0, 0]; // Preserved parcel size

  worker.Buf.writeUint8 = function (value) {
    if (worker.Buf.outgoingIndex >= buf.length) {
      buf.push(value);
    } else {
      buf[worker.Buf.outgoingIndex] = value;
    }

    worker.Buf.outgoingIndex++;
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
 * Verify GsmPDUHelper#readDiallingNumber
 */
add_test(function test_read_dialling_number() {
  let worker = newUint8Worker();
  let helper = worker.GsmPDUHelper;
  let str = "123456789";

  helper.readHexOctet = function () {
    return 0x81;
  };

  helper.readSwappedNibbleBcdString = function (len) {
    return str.substring(0, len);
  };

  for (let i = 0; i < str.length; i++) {
    do_check_eq(str.substring(0, i - 1), // -1 for the TON
                helper.readDiallingNumber(i));
  }

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
    worker.RIL.iccInfoPrivate.sst = sst;
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
 * Verify RIL.iccGetCardLockState("fdn")
 */
add_test(function test_icc_get_card_lock_state_fdn() {
  let worker = newUint8Worker();
  let ril = worker.RIL;
  let buf = worker.Buf;

  buf.sendParcel = function () {
    // Request Type.
    do_check_eq(this.readUint32(), REQUEST_QUERY_FACILITY_LOCK)

    // Token : we don't care.
    this.readUint32();

    // String Array Length.
    do_check_eq(this.readUint32(), worker.RILQUIRKS_V5_LEGACY ? 3 : 4);

    // Facility.
    do_check_eq(this.readString(), ICC_CB_FACILITY_FDN);

    // Password.
    do_check_eq(this.readString(), "");

    // Service class.
    do_check_eq(this.readString(), (ICC_SERVICE_CLASS_VOICE |
                                    ICC_SERVICE_CLASS_DATA  |
                                    ICC_SERVICE_CLASS_FAX).toString());

    if (!worker.RILQUIRKS_V5_LEGACY) {
      // AID. Ignore because it's from modem.
      this.readUint32();
    }

    run_next_test();
  };

  ril.iccGetCardLockState({lockType: "fdn"});
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
    mcc: "466",
    mnc: "92",
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
    mcc: "466",
    mnc: "02",
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
    mcc: "466",
    mnc: "222",
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

/**
 * Verify Proactive Command : Get Input
 */
add_test(function test_stk_proactive_command_get_input() {
  let worker = newUint8Worker();
  let pduHelper = worker.GsmPDUHelper;
  let berHelper = worker.BerTlvHelper;
  let stkHelper = worker.StkProactiveCmdHelper;
  let stkCmdHelper = worker.StkCommandParamsFactory;

  let get_input_1 = [
    0xD0,
    0x1E,
    0x81, 0x03, 0x01, 0x23, 0x8F,
    0x82, 0x02, 0x81, 0x82,
    0x8D, 0x05, 0x04, 0x54, 0x65, 0x78, 0x74,
    0x91, 0x02, 0x01, 0x10,
    0x17, 0x08, 0x04, 0x44, 0x65, 0x66, 0x61, 0x75, 0x6C, 0x74];

  for (let i = 0; i < get_input_1.length; i++) {
    pduHelper.writeHexOctet(get_input_1[i]);
  }

  let berTlv = berHelper.decode(get_input_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_GET_INPUT);

  let input = stkCmdHelper.createParam(tlv.value, ctlvs);
  do_check_eq(input.text, "Text");
  do_check_eq(input.isAlphabet, true);
  do_check_eq(input.isUCS2, true);
  do_check_eq(input.hideInput, true);
  do_check_eq(input.isPacked, true);
  do_check_eq(input.isHelpAvailable, true);
  do_check_eq(input.minLength, 0x01);
  do_check_eq(input.maxLength, 0x10);
  do_check_eq(input.defaultText, "Default");

  let get_input_2 = [
    0xD0,
    0x11,
    0x81, 0x03, 0x01, 0x23, 0x00,
    0x82, 0x02, 0x81, 0x82,
    0x8D, 0x00,
    0x91, 0x02, 0x01, 0x10,
    0x17, 0x00];

  for (let i = 0; i < get_input_2.length; i++) {
    pduHelper.writeHexOctet(get_input_2[i]);
  }

  berTlv = berHelper.decode(get_input_2.length);
  ctlvs = berTlv.value;
  tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_GET_INPUT);

  input = stkCmdHelper.createParam(tlv.value, ctlvs);
  do_check_eq(input.text, null);
  do_check_eq(input.minLength, 0x01);
  do_check_eq(input.maxLength, 0x10);
  do_check_eq(input.defaultText, null);

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
    RIL.iccInfoPrivate.spnDisplayCondition = iccDisplayCondition;
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

/**
 * Verify Proactive Command : Set Up Call
 */
add_test(function test_stk_proactive_command_set_up_call() {
  let worker = newUint8Worker();
  let pduHelper = worker.GsmPDUHelper;
  let berHelper = worker.BerTlvHelper;
  let stkHelper = worker.StkProactiveCmdHelper;
  let cmdFactory = worker.StkCommandParamsFactory;

  let set_up_call_1 = [
    0xD0,
    0x29,
    0x81, 0x03, 0x01, 0x10, 0x04,
    0x82, 0x02, 0x81, 0x82,
    0x05, 0x0A, 0x44, 0x69, 0x73, 0x63, 0x6F, 0x6E, 0x6E, 0x65, 0x63, 0x74,
    0x86, 0x09, 0x81, 0x10, 0x32, 0x04, 0x21, 0x43, 0x65, 0x1C, 0x2C,
    0x05, 0x07, 0x4D, 0x65, 0x73, 0x73, 0x61, 0x67, 0x65];

  for (let i = 0 ; i < set_up_call_1.length; i++) {
    pduHelper.writeHexOctet(set_up_call_1[i]);
  }

  let berTlv = berHelper.decode(set_up_call_1.length);
  let ctlvs = berTlv.value;
  let tlv = stkHelper.searchForTag(COMPREHENSIONTLV_TAG_COMMAND_DETAILS, ctlvs);
  do_check_eq(tlv.value.commandNumber, 0x01);
  do_check_eq(tlv.value.typeOfCommand, STK_CMD_SET_UP_CALL);

  let setupCall = cmdFactory.createParam(tlv.value, ctlvs);
  do_check_eq(setupCall.address, "012340123456,1,2");
  do_check_eq(setupCall.confirmMessage, "Disconnect");
  do_check_eq(setupCall.callMessage, "Message");

  run_next_test();
});

add_test(function test_read_pnn() {
  let worker = newUint8Worker();
  let helper = worker.GsmPDUHelper;
  let record = worker.ICCRecordHelper;
  let buf    = worker.Buf;
  let io     = worker.ICCIOHelper;
  let ril    = worker.RIL;

  io.loadLinearFixedEF = function fakeLoadLinearFixedEF(options) {
    let records = [
      // Record 1 - fullName: 'Long1', shortName: 'Short1'
      [0x43, 0x06, 0x85, 0xCC, 0xB7, 0xFB, 0x1C, 0x03,
       0x45, 0x07, 0x86, 0x53, 0xF4, 0x5B, 0x4E, 0x8F, 0x01,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF],
      // Record 2 - fullName: 'Long2'
      [0x43, 0x06, 0x85, 0xCC, 0xB7, 0xFB, 0x2C, 0x03,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF],
      // Record 3 - Unused bytes
      [0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
       0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF],
    ];

    // Fake get response
    options.totalRecords = records.length;
    options.recordSize = records[0].length;

    options.p1 = options.p1 || 1;

    let record = records[options.p1 - 1];

    // Write data size
    buf.writeUint32(record.length * 2);

    // Write record
    for (let i = 0; i < record.length; i++) {
      helper.writeHexOctet(record[i]);
    }

    // Write string delimiter
    buf.writeStringDelimiter(record.length * 2);

    if (options.callback) {
      options.callback(options);
    }
  };

  io.loadNextRecord = function fakeLoadNextRecord(options) {
    options.p1++;
    io.loadLinearFixedEF(options);
  };

  record.readPNN();

  do_check_eq(ril.iccInfoPrivate.PNN.length, 2);
  do_check_eq(ril.iccInfoPrivate.PNN[0].fullName, "Long1");
  do_check_eq(ril.iccInfoPrivate.PNN[0].shortName, "Short1");
  do_check_eq(ril.iccInfoPrivate.PNN[1].fullName, "Long2");
  do_check_eq(ril.iccInfoPrivate.PNN[1].shortName, undefined);

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

add_test(function test_get_network_name_from_icc() {
  let worker = newUint8Worker();
  let RIL = worker.RIL;
  let ICCUtilsHelper = worker.ICCUtilsHelper;

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
  let buf = worker.Buf;
  let pduHelper = worker.GsmPDUHelper;

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

  let pbr = worker.ICCUtilsHelper.parsePbrTlvs(pbrTlvs);
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
 * Verify Event Download Command : Location Status
 */
add_test(function test_stk_event_download_location_status() {
  let worker = newUint8SupportOutgoingIndexWorker();
  let buf = worker.Buf;
  let pduHelper = worker.GsmPDUHelper;

  buf.sendParcel = function () {
    // Type
    do_check_eq(this.readUint32(), REQUEST_STK_SEND_ENVELOPE_COMMAND);

    // Token : we don't care
    this.readUint32();

    // Data Size, 42 = 2 * (2 + TLV_DEVICE_ID_SIZE(4) +
    //                      TLV_EVENT_LIST_SIZE(3) +
    //                      TLV_LOCATION_STATUS_SIZE(3) +
    //                      TLV_LOCATION_INFO_GSM_SIZE(9))
    do_check_eq(this.readUint32(), 42);

    // BER tag
    do_check_eq(pduHelper.readHexOctet(), BER_EVENT_DOWNLOAD_TAG);

    // BER length, 19 = TLV_DEVICE_ID_SIZE(4) +
    //                  TLV_EVENT_LIST_SIZE(3) +
    //                  TLV_LOCATION_STATUS_SIZE(3) +
    //                  TLV_LOCATION_INFO_GSM_SIZE(9)
    do_check_eq(pduHelper.readHexOctet(), 19);

    // Device Identifies, Type-Length-Value(Source ID-Destination ID)
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 2);
    do_check_eq(pduHelper.readHexOctet(), STK_DEVICE_ID_ME);
    do_check_eq(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

    // Event List, Type-Length-Value
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_EVENT_LIST |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 1);
    do_check_eq(pduHelper.readHexOctet(), STK_EVENT_TYPE_LOCATION_STATUS);

    // Location Status, Type-Length-Value
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_LOCATION_STATUS |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 1);
    do_check_eq(pduHelper.readHexOctet(), STK_SERVICE_STATE_NORMAL);

    // Location Info, Type-Length-Value
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_LOCATION_INFO |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 7);

    do_check_eq(pduHelper.readHexOctet(), 0x21); // MCC + MNC
    do_check_eq(pduHelper.readHexOctet(), 0x63);
    do_check_eq(pduHelper.readHexOctet(), 0x54);
    do_check_eq(pduHelper.readHexOctet(), 0); // LAC
    do_check_eq(pduHelper.readHexOctet(), 0);
    do_check_eq(pduHelper.readHexOctet(), 0); // Cell ID
    do_check_eq(pduHelper.readHexOctet(), 0);

    run_next_test();
  };

  let event = {
    eventType: STK_EVENT_TYPE_LOCATION_STATUS,
    locationStatus: STK_SERVICE_STATE_NORMAL,
    locationInfo: {
      mcc: "123",
      mnc: "456",
      gsmLocationAreaCode: 0,
      gsmCellId: 0
    }
  };
  worker.RIL.sendStkEventDownload({
    event: event
  });
});

/**
 * Verify STK terminal response
 */
add_test(function test_stk_terminal_response() {
  let worker = newUint8SupportOutgoingIndexWorker();
  let buf = worker.Buf;
  let pduHelper = worker.GsmPDUHelper;

  buf.sendParcel = function () {
    // Type
    do_check_eq(this.readUint32(), REQUEST_STK_SEND_TERMINAL_RESPONSE);

    // Token : we don't care
    this.readUint32();

    // Data Size, 44 = 2 * (TLV_COMMAND_DETAILS_SIZE(5) +
    //                      TLV_DEVICE_ID_SIZE(4) +
    //                      TLV_RESULT_SIZE(3) +
    //                      TEXT LENGTH(10))
    do_check_eq(this.readUint32(), 44);

    // Command Details, Type-Length-Value
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_COMMAND_DETAILS |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 3);
    do_check_eq(pduHelper.readHexOctet(), 0x01);
    do_check_eq(pduHelper.readHexOctet(), STK_CMD_PROVIDE_LOCAL_INFO);
    do_check_eq(pduHelper.readHexOctet(), STK_LOCAL_INFO_NNA);

    // Device Identifies, Type-Length-Value(Source ID-Destination ID)
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID);
    do_check_eq(pduHelper.readHexOctet(), 2);
    do_check_eq(pduHelper.readHexOctet(), STK_DEVICE_ID_ME);
    do_check_eq(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

    // Result
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_RESULT |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 1);
    do_check_eq(pduHelper.readHexOctet(), STK_RESULT_OK);

    // Text
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_TEXT_STRING |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 8);
    do_check_eq(pduHelper.readHexOctet(), STK_TEXT_CODING_GSM_7BIT_PACKED);
    do_check_eq(pduHelper.readSeptetsToString(7, 0, PDU_NL_IDENTIFIER_DEFAULT,
                PDU_NL_IDENTIFIER_DEFAULT), "Mozilla");

    run_next_test();
  };

  let response = {
    command: {
      commandNumber: 0x01,
      typeOfCommand: STK_CMD_PROVIDE_LOCAL_INFO,
      commandQualifier: STK_LOCAL_INFO_NNA,
      options: {
        isPacked: true
      }
    },
    input: "Mozilla",
    resultCode: STK_RESULT_OK
  };
  worker.RIL.sendStkTerminalResponse(response);
});

/**
 * Verify Event Download Command : Language Selection
 */
add_test(function test_stk_event_download_language_selection() {
  let worker = newUint8SupportOutgoingIndexWorker();
  let buf = worker.Buf;
  let pduHelper = worker.GsmPDUHelper;

  buf.sendParcel = function () {
    // Type
    do_check_eq(this.readUint32(), REQUEST_STK_SEND_ENVELOPE_COMMAND);

    // Token : we don't care
    this.readUint32();

    // Data Size, 26 = 2 * (2 + TLV_DEVICE_ID_SIZE(4) +
    //                      TLV_EVENT_LIST_SIZE(3) +
    //                      TLV_LANGUAGE(4))
    do_check_eq(this.readUint32(), 26);

    // BER tag
    do_check_eq(pduHelper.readHexOctet(), BER_EVENT_DOWNLOAD_TAG);

    // BER length, 19 = TLV_DEVICE_ID_SIZE(4) +
    //                  TLV_EVENT_LIST_SIZE(3) +
    //                  TLV_LANGUAGE(4)
    do_check_eq(pduHelper.readHexOctet(), 11);

    // Device Identifies, Type-Length-Value(Source ID-Destination ID)
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 2);
    do_check_eq(pduHelper.readHexOctet(), STK_DEVICE_ID_ME);
    do_check_eq(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

    // Event List, Type-Length-Value
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_EVENT_LIST |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 1);
    do_check_eq(pduHelper.readHexOctet(), STK_EVENT_TYPE_LANGUAGE_SELECTION);

    // Language, Type-Length-Value
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_LANGUAGE);
    do_check_eq(pduHelper.readHexOctet(), 2);
    do_check_eq(pduHelper.read8BitUnpackedToString(2), "zh");

    run_next_test();
  };

  let event = {
    eventType: STK_EVENT_TYPE_LANGUAGE_SELECTION,
    language: "zh"
  };
  worker.RIL.sendStkEventDownload({
    event: event
  });
});

/**
 * Verify Event Download Command : Idle Screen Available
 */
add_test(function test_stk_event_download_idle_screen_available() {
  let worker = newUint8SupportOutgoingIndexWorker();
  let buf = worker.Buf;
  let pduHelper = worker.GsmPDUHelper;

  buf.sendParcel = function () {
    // Type
    do_check_eq(this.readUint32(), REQUEST_STK_SEND_ENVELOPE_COMMAND);

    // Token : we don't care
    this.readUint32();

    // Data Size, 18 = 2 * (2 + TLV_DEVICE_ID_SIZE(4) + TLV_EVENT_LIST_SIZE(3))
    do_check_eq(this.readUint32(), 18);

    // BER tag
    do_check_eq(pduHelper.readHexOctet(), BER_EVENT_DOWNLOAD_TAG);

    // BER length, 7 = TLV_DEVICE_ID_SIZE(4) + TLV_EVENT_LIST_SIZE(3)
    do_check_eq(pduHelper.readHexOctet(), 7);

    // Device Identities, Type-Length-Value(Source ID-Destination ID)
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_DEVICE_ID |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 2);
    do_check_eq(pduHelper.readHexOctet(), STK_DEVICE_ID_DISPLAY);
    do_check_eq(pduHelper.readHexOctet(), STK_DEVICE_ID_SIM);

    // Event List, Type-Length-Value
    do_check_eq(pduHelper.readHexOctet(), COMPREHENSIONTLV_TAG_EVENT_LIST |
                                          COMPREHENSIONTLV_FLAG_CR);
    do_check_eq(pduHelper.readHexOctet(), 1);
    do_check_eq(pduHelper.readHexOctet(), STK_EVENT_TYPE_IDLE_SCREEN_AVAILABLE);

    run_next_test();
  };

  let event = {
    eventType: STK_EVENT_TYPE_IDLE_SCREEN_AVAILABLE
  };
  worker.RIL.sendStkEventDownload({
    event: event
  });
});

/**
 * Verify ICCIOHelper.loadLinearFixedEF with recordSize.
 */
add_test(function test_load_linear_fixed_ef() {
  let worker = newUint8Worker();
  let ril = worker.RIL;
  let io = worker.ICCIOHelper;

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
  let ril = worker.RIL;
  let io = worker.ICCIOHelper;

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
  let helper = worker.GsmPDUHelper;
  let record = worker.ICCRecordHelper;
  let buf    = worker.Buf;
  let io     = worker.ICCIOHelper;

  io.loadLinearFixedEF = function fakeLoadLinearFixedEF(options)  {
    let pbr_1 = [
      0xa8, 0x05, 0xc0, 0x03, 0x4f, 0x3a, 0x01
    ];

    // Write data size
    buf.writeUint32(pbr_1.length * 2);

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
    run_next_test();
  };

  let errorCb = function errorCb(errorMsg) {
    do_print("Reading EF_PBR failed, msg = " + errorMsg);
    do_check_true(false);
    run_next_test();
  };

  record.readPBR(successCb, errorCb);
});

/**
 * Verify ICCRecordHelper.readEmail
 */
add_test(function test_read_email() {
  let worker = newUint8Worker();
  let helper = worker.GsmPDUHelper;
  let record = worker.ICCRecordHelper;
  let buf    = worker.Buf;
  let io     = worker.ICCIOHelper;
  let recordSize;

  io.loadLinearFixedEF = function fakeLoadLinearFixedEF(options)  {
    let email_1 = [
      0x65, 0x6D, 0x61, 0x69, 0x6C,
      0x00, 0x6D, 0x6F, 0x7A, 0x69,
      0x6C, 0x6C, 0x61, 0x2E, 0x63,
      0x6F, 0x6D, 0x02, 0x23];

    // Write data size
    buf.writeUint32(email_1.length * 2);

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
    record.readEmail(fileId, type, recordNumber, function (email) {
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
  let pduHelper = worker.GsmPDUHelper;
  let ril = worker.RIL;
  ril.appType = CARD_APPTYPE_USIM;
  let recordHelper = worker.ICCRecordHelper;
  let buf = worker.Buf;
  let ioHelper = worker.ICCIOHelper;
  let pbr = {email: {fileId: fileId, fileType: ICC_USIM_TYPE1_TAG},
             adn: {sfi: 1}};
  let count = 0;

  // Override.
  ioHelper.updateLinearFixedEF = function (options) {
    options.pathId = worker.ICCFileHelper.getEFPath(options.fileId);
    options.command = ICC_COMMAND_UPDATE_RECORD;
    options.p1 = options.recordNumber;
    options.p2 = READ_RECORD_ABSOLUTE_MODE;
    options.p3 = recordSize;
    ril.iccIO(options);
  };

  function do_test(pbr, expectedEmail, expectedAdnRecordId) {
    buf.sendParcel = function () {
      count++;

      // Request Type.
      do_check_eq(this.readUint32(), REQUEST_SIM_IO);

      // Token : we don't care
      this.readUint32();

      // command.
      do_check_eq(this.readUint32(), ICC_COMMAND_UPDATE_RECORD);

      // fileId.
      do_check_eq(this.readUint32(), fileId);

      // pathId.
      do_check_eq(this.readString(),
                  EF_PATH_MF_SIM + EF_PATH_DF_TELECOM + EF_PATH_DF_PHONEBOOK);

      // p1.
      do_check_eq(this.readUint32(), recordNumber);

      // p2.
      do_check_eq(this.readUint32(), READ_RECORD_ABSOLUTE_MODE);

      // p3.
      do_check_eq(this.readUint32(), recordSize);

      // data.
      let strLen = this.readUint32();
      let email;
      if (pbr.email.fileType === ICC_USIM_TYPE1_TAG) {
        email = pduHelper.read8BitUnpackedToString(recordSize);
      } else {
        email = pduHelper.read8BitUnpackedToString(recordSize - 2);
        do_check_eq(pduHelper.readHexOctet(), pbr.adn.sfi);
        do_check_eq(pduHelper.readHexOctet(), expectedAdnRecordId);
      }
      this.readStringDelimiter(strLen);
      do_check_eq(email, expectedEmail);

      // pin2.
      do_check_eq(this.readString(), null);

      if (!worker.RILQUIRKS_V5_LEGACY) {
        // AID. Ignore because it's from modem.
        this.readUint32();
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
  let helper = worker.GsmPDUHelper;
  let record = worker.ICCRecordHelper;
  let buf    = worker.Buf;
  let io     = worker.ICCIOHelper;
  let recordSize;

  io.loadLinearFixedEF = function fakeLoadLinearFixedEF(options)  {
    let anr_1 = [
      0x01, 0x05, 0x81, 0x10, 0x32,
      0x54, 0xF6, 0xFF, 0xFF];

    // Write data size
    buf.writeUint32(anr_1.length * 2);

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
    record.readANR(fileId, fileType, recordNumber, function (anr) {
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
  let pduHelper = worker.GsmPDUHelper;
  let ril = worker.RIL;
  ril.appType = CARD_APPTYPE_USIM;
  let recordHelper = worker.ICCRecordHelper;
  let buf = worker.Buf;
  let ioHelper = worker.ICCIOHelper;
  let pbr = {anr0: {fileId: fileId, fileType: ICC_USIM_TYPE1_TAG},
             adn: {sfi: 1}};
  let count = 0;

  // Override.
  ioHelper.updateLinearFixedEF = function (options) {
    options.pathId = worker.ICCFileHelper.getEFPath(options.fileId);
    options.command = ICC_COMMAND_UPDATE_RECORD;
    options.p1 = options.recordNumber;
    options.p2 = READ_RECORD_ABSOLUTE_MODE;
    options.p3 = recordSize;
    ril.iccIO(options);
  };

  function do_test(pbr, expectedANR, expectedAdnRecordId) {
    buf.sendParcel = function () {
      count++;

      // Request Type.
      do_check_eq(this.readUint32(), REQUEST_SIM_IO);

      // Token : we don't care
      this.readUint32();

      // command.
      do_check_eq(this.readUint32(), ICC_COMMAND_UPDATE_RECORD);

      // fileId.
      do_check_eq(this.readUint32(), fileId);

      // pathId.
      do_check_eq(this.readString(),
                  EF_PATH_MF_SIM + EF_PATH_DF_TELECOM + EF_PATH_DF_PHONEBOOK);

      // p1.
      do_check_eq(this.readUint32(), recordNumber);

      // p2.
      do_check_eq(this.readUint32(), READ_RECORD_ABSOLUTE_MODE);

      // p3.
      do_check_eq(this.readUint32(), recordSize);

      // data.
      let strLen = this.readUint32();
      // EF_AAS, ignore.
      pduHelper.readHexOctet();
      do_check_eq(pduHelper.readNumberWithLength(), expectedANR);
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

      if (!worker.RILQUIRKS_V5_LEGACY) {
        // AID. Ignore because it's from modem.
        this.readUint32();
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
  let helper = worker.GsmPDUHelper;
  let record = worker.ICCRecordHelper;
  let buf    = worker.Buf;
  let io     = worker.ICCIOHelper;
  let recordSize;

  io.loadLinearFixedEF = function fakeLoadLinearFixedEF(options)  {
    let iap_1 = [0x01, 0x02];

    // Write data size/
    buf.writeUint32(iap_1.length * 2);

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
  let pduHelper = worker.GsmPDUHelper;
  let ril = worker.RIL;
  ril.appType = CARD_APPTYPE_USIM;
  let recordHelper = worker.ICCRecordHelper;
  let buf = worker.Buf;
  let ioHelper = worker.ICCIOHelper;
  let count = 0;

  // Override.
  ioHelper.updateLinearFixedEF = function (options) {
    options.pathId = worker.ICCFileHelper.getEFPath(options.fileId);
    options.command = ICC_COMMAND_UPDATE_RECORD;
    options.p1 = options.recordNumber;
    options.p2 = READ_RECORD_ABSOLUTE_MODE;
    options.p3 = recordSize;
    ril.iccIO(options);
  };

  function do_test(expectedIAP) {
    buf.sendParcel = function () {
      // Request Type.
      do_check_eq(this.readUint32(), REQUEST_SIM_IO);

      // Token : we don't care
      this.readUint32();

      // command.
      do_check_eq(this.readUint32(), ICC_COMMAND_UPDATE_RECORD);

      // fileId.
      do_check_eq(this.readUint32(), fileId);

      // pathId.
      do_check_eq(this.readString(),
                  EF_PATH_MF_SIM + EF_PATH_DF_TELECOM + EF_PATH_DF_PHONEBOOK);

      // p1.
      do_check_eq(this.readUint32(), recordNumber);

      // p2.
      do_check_eq(this.readUint32(), READ_RECORD_ABSOLUTE_MODE);

      // p3.
      do_check_eq(this.readUint32(), recordSize);

      // data.
      let strLen = this.readUint32();
      for (let i = 0; i < recordSize; i++) {
        do_check_eq(expectedIAP[i], pduHelper.readHexOctet());
      }
      this.readStringDelimiter(strLen);

      // pin2.
      do_check_eq(this.readString(), null);

      if (!worker.RILQUIRKS_V5_LEGACY) {
        // AID. Ignore because it's from modem.
        this.readUint32();
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
  let ril = worker.RIL;
  let record = worker.ICCRecordHelper;
  let io = worker.ICCIOHelper;
  let pdu = worker.GsmPDUHelper;
  let buf = worker.Buf;

  ril.appType = CARD_APPTYPE_SIM;
  const recordSize = 0x20;
  let fileId;

  // Override.
  io.updateLinearFixedEF = function (options) {
    options.pathId = worker.ICCFileHelper.getEFPath(options.fileId);
    options.command = ICC_COMMAND_UPDATE_RECORD;
    options.p1 = options.recordNumber;
    options.p2 = READ_RECORD_ABSOLUTE_MODE;
    options.p3 = recordSize;
    ril.iccIO(options);
  };

  buf.sendParcel = function () {
    // Request Type.
    do_check_eq(this.readUint32(), REQUEST_SIM_IO);

    // Token : we don't care
    this.readUint32();

    // command.
    do_check_eq(this.readUint32(), ICC_COMMAND_UPDATE_RECORD);

    // fileId.
    do_check_eq(this.readUint32(), fileId);

    // pathId.
    do_check_eq(this.readString(), EF_PATH_MF_SIM + EF_PATH_DF_TELECOM);

    // p1.
    do_check_eq(this.readUint32(), 1);

    // p2.
    do_check_eq(this.readUint32(), READ_RECORD_ABSOLUTE_MODE);

    // p3.
    do_check_eq(this.readUint32(), 0x20);

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

    if (!worker.RILQUIRKS_V5_LEGACY) {
      // AID. Ignore because it's from modem.
      this.readUint32();
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
  let pduHelper = worker.GsmPDUHelper;
  let recordHelper = worker.ICCRecordHelper;
  let buf = worker.Buf;
  let io  = worker.ICCIOHelper;

  function writeRecord (record) {
    // Write data size
    buf.writeUint32(record.length * 2);

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
    function (recordId) {
      do_check_eq(recordId, 2);
      run_next_test();
    }.bind(this),
    function (errorMsg) {
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
  let record = worker.ICCRecordHelper;
  let contactHelper = worker.ICCContactHelper;

  function do_test(aSimType, aContactType, aExpectedContact, aEnhancedPhoneBook) {
    worker.RIL.appType = aSimType;
    worker.RIL._isCdma = (aSimType === CARD_APPTYPE_RUIM);
    worker.RIL.iccInfoPrivate.cst = (aEnhancedPhoneBook) ?
                                    [0x0, 0x0C, 0x0, 0x0, 0x0]:
                                    [0x0, 0x00, 0x0, 0x0, 0x0];

    // Override some functions to test.
    contactHelper.getContactFieldRecordId = function (pbr, contact, field, onsuccess, onerror) {
      onsuccess(1);
    };

    record.readPBR = function readPBR(onsuccess, onerror) {
      onsuccess([{adn:{}, email: {}, anr0: {}}]);
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
      for (key in contact) {
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
    recordId: 1,
    alphaId:  "name",
    number:   "111111"
  };

  let expectedContact2 = {
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
  let recordHelper = worker.ICCRecordHelper;
  let contactHelper = worker.ICCContactHelper;

  function do_test(aSimType, aContactType, aContact, aPin2, aFileType, aEnhancedPhoneBook) {
    worker.RIL.appType = aSimType;
    worker.RIL._isCdma = (aSimType === CARD_APPTYPE_RUIM);
    worker.RIL.iccInfoPrivate.cst = (aEnhancedPhoneBook) ?
                                    [0x0, 0x0C, 0x0, 0x0, 0x0]:
                                    [0x0, 0x00, 0x0, 0x0, 0x0];

    recordHelper.readPBR = function (onsuccess, onerror) {
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

    recordHelper.updateADNLike = function (fileId, contact, pin2, onsuccess, onerror) {
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

    recordHelper.readIAP = function (fileId, recordNumber, onsuccess, onerror) {
      do_check_eq(fileId, IAP_FILE_ID);
      do_check_eq(recordNumber, ADN_RECORD_ID);
      onsuccess([EMAIL_RECORD_ID, ANR0_RECORD_ID]);
    };

    recordHelper.updateEmail = function (pbr, recordNumber, email, adnRecordId, onsuccess, onerror) {
      do_check_eq(pbr.email.fileId, EMAIL_FILE_ID);
      if (pbr.email.fileType === ICC_USIM_TYPE1_TAG) {
        do_check_eq(recordNumber, ADN_RECORD_ID);
      } else if (pbr.email.fileType === ICC_USIM_TYPE2_TAG) {
        do_check_eq(recordNumber, EMAIL_RECORD_ID);
      }
      do_check_eq(email, aContact.email);
      onsuccess();
    };

    recordHelper.updateANR = function (pbr, recordNumber, number, adnRecordId, onsuccess, onerror) {
      do_check_eq(pbr.anr0.fileId, ANR0_FILE_ID);
      if (pbr.anr0.fileType === ICC_USIM_TYPE1_TAG) {
        do_check_eq(recordNumber, ADN_RECORD_ID);
      } else if (pbr.anr0.fileType === ICC_USIM_TYPE2_TAG) {
        do_check_eq(recordNumber, ANR0_RECORD_ID);
      }
      do_check_eq(number, aContact.anr[0]);
      onsuccess();
    };

    let onsuccess = function onsuccess() {
      do_print("updateICCContact success");
    };

    let onerror = function onerror(errorMsg) {
      do_print("updateICCContact failed: " + errorMsg);
      do_check_true(false);
    };

    contactHelper.updateICCContact(aSimType, aContactType, aContact, aPin2, onsuccess, onerror);
  }

  let contact = {
    recordId: ADN_RECORD_ID,
    alphaId:  "test",
    number:   "123456",
    email:    "test@mail.com",
    anr:      ["+654321"]
  };

  // SIM
  do_print("Test update SIM adn contacts");
  do_test(CARD_APPTYPE_SIM, "adn", contact);

  do_print("Test update SIM fdn contacts");
  do_test(CARD_APPTYPE_SIM, "fdn", contact, "1234");

  // USIM
  do_print("Test update USIM adn contacts");
  do_test(CARD_APPTYPE_USIM, "adn", contact, null, ICC_USIM_TYPE1_TAG);
  do_test(CARD_APPTYPE_USIM, "adn", contact, null, ICC_USIM_TYPE2_TAG);

  do_print("Test update USIM fdn contacts");
  do_test(CARD_APPTYPE_USIM, "fdn", contact, "1234");

  // RUIM
  do_print("Test update RUIM adn contacts");
  do_test(CARD_APPTYPE_RUIM, "adn", contact);

  do_print("Test update RUIM fdn contacts");
  do_test(CARD_APPTYPE_RUIM, "fdn", contact, "1234");

  // RUIM with enhanced phone book
  do_print("Test update RUIM adn contacts with enhanced phone book");
  do_test(CARD_APPTYPE_RUIM, "adn", contact, null, ICC_USIM_TYPE1_TAG, true);
  do_test(CARD_APPTYPE_RUIM, "adn", contact, null, ICC_USIM_TYPE2_TAG, true);

  do_print("Test update RUIM fdn contacts with enhanced phone book");
  do_test(CARD_APPTYPE_RUIM, "fdn", contact, "1234", null, true);

  run_next_test();
});

/**
 * Verify ICCContactHelper.findFreeICCContact
 */
add_test(function test_find_free_icc_contact() {
  let worker = newUint8Worker();
  let recordHelper = worker.ICCRecordHelper;
  let contactHelper = worker.ICCContactHelper;
  const RECORD_ID = 1;

  recordHelper.readPBR = function (onsuccess, onerror) {
    onsuccess([{adn:{}, email: {}, anr0: {}}]);
  };

  recordHelper.findFreeRecordId = function (fileId, onsuccess, onerror) {
    onsuccess(RECORD_ID);
  };

  let successCb = function (recordId) {
    do_check_eq(recordId, RECORD_ID);
    run_next_test();
  };

  let errorCb = function (errorMsg) {
    do_check_true(false);
    run_next_test();
  };

  contactHelper.findFreeICCContact(CARD_APPTYPE_USIM, "adn", successCb, errorCb);
});

add_test(function test_personalization_state() {
  let worker = newUint8Worker();
  let ril = worker.RIL;

  function testPersonalization(cardPersoState, geckoCardState) {
    let iccStatus = {
      gsmUmtsSubscriptionAppIndex: 0,
      apps: [
        {
          app_state: CARD_APPSTATE_SUBSCRIPTION_PERSO,
          perso_substate: cardPersoState
        }],
    };

    ril._processICCStatus(iccStatus);
    do_check_eq(ril.cardState, geckoCardState);
  }

  testPersonalization(CARD_PERSOSUBSTATE_SIM_NETWORK,
                      GECKO_CARDSTATE_NETWORK_LOCKED);
  testPersonalization(CARD_PERSOSUBSTATE_SIM_CORPORATE,
                      GECKO_CARDSTATE_CORPORATE_LOCKED);
  testPersonalization(CARD_PERSOSUBSTATE_SIM_SERVICE_PROVIDER,
                      GECKO_CARDSTATE_SERVICE_PROVIDER_LOCKED);
  testPersonalization(CARD_PERSOSUBSTATE_SIM_NETWORK_PUK,
                      GECKO_CARDSTATE_NETWORK_PUK_REQUIRED);
  testPersonalization(CARD_PERSOSUBSTATE_SIM_CORPORATE_PUK,
                      GECKO_CARDSTATE_CORPORATE_PUK_REQUIRED);
  testPersonalization(CARD_PERSOSUBSTATE_SIM_SERVICE_PROVIDER_PUK,
                      GECKO_CARDSTATE_SERVICE_PROVIDER_PUK_REQUIRED);
  testPersonalization(CARD_PERSOSUBSTATE_READY,
                      GECKO_CARDSTATE_PERSONALIZATION_READY);

  run_next_test();
});

/**
 * Verify iccSetCardLock - Facility Lock.
 */
add_test(function test_set_icc_card_lock_facility_lock() {
  let worker = newUint8Worker();
  worker.RILQUIRKS_V5_LEGACY = false;
  let aid = "123456789";
  let ril = worker.RIL;
  ril.aid = aid;
  let buf = worker.Buf;

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
      do_check_eq(this.readUint32(), REQUEST_SET_FACILITY_LOCK);

      // Token : we don't care
      this.readUint32();

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
  let ril = worker.RIL;
  let buf = worker.Buf;
  const pin = "12345678";
  const puk = "12345678";

  let GECKO_CARDLOCK_TO_PASSWORD_TYPE = {};
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_NCK] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_CCK] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_SPCK] = "pin";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_NCK_PUK] = "puk";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_CCK_PUK] = "puk";
  GECKO_CARDLOCK_TO_PASSWORD_TYPE[GECKO_CARDLOCK_SPCK_PUK] = "puk";

  function do_test(aLock, aPassword) {
    buf.sendParcel = function fakeSendParcel () {
      // Request Type.
      do_check_eq(this.readUint32(), REQUEST_ENTER_NETWORK_DEPERSONALIZATION_CODE);

      // Token : we don't care
      this.readUint32();

      let lockType = GECKO_PERSO_LOCK_TO_CARD_PERSO_LOCK[aLock];
      // Lock Type
      do_check_eq(this.readUint32(), lockType);

      // Pin/Puk.
      do_check_eq(this.readString(), aPassword);
    };

    let lock = {lockType: aLock};
    lock[GECKO_CARDLOCK_TO_PASSWORD_TYPE[aLock]] = aPassword;
    ril.iccUnlockCardLock(lock);
  }

  do_test(GECKO_CARDLOCK_NCK, pin);
  do_test(GECKO_CARDLOCK_CCK, pin);
  do_test(GECKO_CARDLOCK_SPCK, pin);
  do_test(GECKO_CARDLOCK_NCK_PUK, puk);
  do_test(GECKO_CARDLOCK_CCK_PUK, puk);
  do_test(GECKO_CARDLOCK_SPCK_PUK, puk);

  run_next_test();
});

/**
 * Verify MCC and MNC parsing
 */
add_test(function test_mcc_mnc_parsing() {
  let worker = newUint8Worker();
  let helper = worker.ICCUtilsHelper;

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
  let record = worker.ICCRecordHelper;
  let helper = worker.GsmPDUHelper;
  let ril    = worker.RIL;
  let buf    = worker.Buf;
  let io     = worker.ICCIOHelper;

  function do_test(mncLengthInEf, imsi, expectedMcc, expectedMnc) {
    ril.iccInfoPrivate.imsi = imsi;

    io.loadTransparentEF = function fakeLoadTransparentEF(options) {
      let ad = [0x00, 0x00, 0x00];
      if (mncLengthInEf) {
        ad.push(mncLengthInEf);
      }

      // Write data size
      buf.writeUint32(ad.length * 2);

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
  do_test(0x03,      "466923202422409", "466", "923");
  do_test(undefined, "310260542718417", "310", "260");
  do_test(0x02,      "310260542718417", "310", "26" );

  run_next_test();
});
