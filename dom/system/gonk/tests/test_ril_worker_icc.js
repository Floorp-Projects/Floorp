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
  let buf = worker.Buf;
  const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];

  // Only write characters before PDU_NL_EXTENDED_ESCAPE to simplify test.
  for (let i = 0; i < PDU_NL_EXTENDED_ESCAPE; i++) {
    helper.writeHexOctet(i);
  }

  // Also write two unused fields.
  let ffLen = 2;
  for (let i = 0; i < ffLen; i++) {
    helper.writeHexOctet(0xff);
  }

  do_check_eq(helper.read8BitUnpackedToString(PDU_NL_EXTENDED_ESCAPE + ffLen),
              langTable.substring(0, PDU_NL_EXTENDED_ESCAPE));
  run_next_test();
});

/**
 * Verify isICCServiceAvailable.
 */
add_test(function test_is_icc_service_available() {
  let worker = newUint8Worker();

  function test_table(sst, geckoService, simEnabled, usimEnabled) {
    worker.RIL.iccInfo.sst = sst;
    worker.RIL.appType = CARD_APPTYPE_SIM;
    do_check_eq(worker.RIL.isICCServiceAvailable(geckoService), simEnabled);
    worker.RIL.appType = CARD_APPTYPE_USIM;
    do_check_eq(worker.RIL.isICCServiceAvailable(geckoService), usimEnabled);
  }

  test_table([0x08], "ADN", true, false);
  test_table([0x08], "FDN", false, false);
  test_table([0x08], "SDN", false, true);

  run_next_test();
});

/**
 * Verify writeDiallingNumber
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

  cellId = (pduHelper.readHexOctet() << 8)  |
               (pduHelper.readHexOctet());
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

  cellId = (pduHelper.readHexOctet() << 8) |
           (pduHelper.readHexOctet());
  do_check_eq(cellId, 65534);

  run_next_test();
});
