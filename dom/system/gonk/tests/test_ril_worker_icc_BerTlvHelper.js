/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

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
  let iter = berTlv.value.values();
  let tlv = berHelper.searchForNextTag(BER_FCP_FILE_DESCRIPTOR_TAG, iter);
  equal(tlv.value.fileStructure, UICC_EF_STRUCTURE[EF_STRUCTURE_TRANSPARENT]);

  tlv = berHelper.searchForNextTag(BER_FCP_FILE_IDENTIFIER_TAG, iter);
  equal(tlv.value.fileId, 0x2FE2);

  tlv = berHelper.searchForNextTag(BER_FCP_FILE_SIZE_DATA_TAG, iter);
  equal(tlv.value.fileSizeData, 0x0A);

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
  let iter = berTlv.value.values();
  let tlv = berHelper.searchForNextTag(BER_FCP_FILE_DESCRIPTOR_TAG, iter);
  equal(tlv.value.fileStructure, UICC_EF_STRUCTURE[EF_STRUCTURE_LINEAR_FIXED]);
  equal(tlv.value.recordLength, 0x1A);
  equal(tlv.value.numOfRecords, 0x01);

  tlv = berHelper.searchForNextTag(BER_FCP_FILE_IDENTIFIER_TAG, iter);
  equal(tlv.value.fileId, 0x6F40);

  tlv = berHelper.searchForNextTag(BER_FCP_FILE_SIZE_DATA_TAG, iter);
  equal(tlv.value.fileSizeData, 0x1A);

  run_next_test();
});
