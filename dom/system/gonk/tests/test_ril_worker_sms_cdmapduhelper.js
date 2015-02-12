/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

/**
 * Verify CdmaPDUHelper#encodeUserDataReplyOption.
 */
add_test(function test_CdmaPDUHelper_encodeUserDataReplyOption() {
  let worker = newWorker({
    postRILMessage: function(data) {
      // Do nothing
    },
    postMessage: function(message) {
      // Do nothing
    }
  });
  let context = worker.ContextPool._contexts[0];

  let testDataBuffer = [];
  context.BitBufferHelper.startWrite(testDataBuffer);

  let helper = context.CdmaPDUHelper;
  helper.encodeUserDataReplyOption({requestStatusReport: true});

  let expectedDataBuffer = [PDU_CDMA_MSG_USERDATA_REPLY_OPTION, 0x01, 0x40];

  equal(testDataBuffer.length, expectedDataBuffer.length);

  for (let i = 0; i < expectedDataBuffer.length; i++) {
    equal(testDataBuffer[i], expectedDataBuffer[i]);
  }

  run_next_test();
});

/**
 * Verify CdmaPDUHelper#cdma_decodeUserDataMsgStatus.
 */
add_test(function test_CdmaPDUHelper_decodeUserDataMsgStatus() {
  let worker = newWorker({
    postRILMessage: function(data) {
      // Do nothing
    },
    postMessage: function(message) {
      // Do nothing
    }
  });
  let context = worker.ContextPool._contexts[0];

  let helper = context.CdmaPDUHelper;
  function test_MsgStatus(octet) {
    let testDataBuffer = [octet];
    context.BitBufferHelper.startRead(testDataBuffer);
    let result = helper.decodeUserDataMsgStatus();

    equal(result.errorClass, octet >>> 6);
    equal(result.msgStatus, octet & 0x3F);
  }

  // 00|000000: no error|Message accepted
  test_MsgStatus(0x00);

  // 10|000100: temporary condition|Network congestion
  test_MsgStatus(0x84);

  // 11|000101: permanent condition|Network error
  test_MsgStatus(0xC5);

  run_next_test();
});

/**
 * Verify CdmaPDUHelper#decodeCdmaPDUMsg.
 *  - encoding by shift-jis
 */
add_test(function test_CdmaPDUHelper_decodeCdmaPDUMsg_Shift_jis() {
  let worker = newWorker({
    postRILMessage: function(data) {
      // Do nothing
    },
    postMessage: function(message) {
      // Do nothing
    }
  });
  let context = worker.ContextPool._contexts[0];

  let helper = context.CdmaPDUHelper;
  function test_decodePDUMsg(testDataBuffer, expected, encoding, msgType, msgBodySize) {
    context.BitBufferHelper.startRead(testDataBuffer);
    let result = helper.decodeCdmaPDUMsg(encoding, msgType, msgBodySize);
    equal(result, expected);
  }

  // Shift-JIS has 1 byte and 2 byte code for one character and has some types of characters:
  //   Hiragana, Kanji, Katakana(fullwidth, halfwidth)...
  // This test is a combination of 1 byte and 2 byte code and types of characters.

  // test case 1
  let testDataBuffer1 = [0x82, 0x58, 0x33, 0x41, 0x61, 0x33, 0x82, 0x60,
                         0x82, 0x81, 0x33, 0xB1, 0xAF, 0x33, 0x83, 0x41,
                         0x83, 0x96, 0x33, 0x82, 0xA0, 0x33, 0x93, 0xFA,
                         0x33, 0x3A, 0x3C, 0x33, 0x81, 0x80, 0x81, 0x8E,
                         0x33, 0x31, 0x82, 0x51, 0x41, 0x61, 0x82, 0x51,
                         0x82, 0x60, 0x82, 0x81, 0x82, 0x51, 0xB1, 0xAF,
                         0x82, 0x51, 0x83, 0x41, 0x83, 0x96, 0x82, 0x51,
                         0x82, 0xA0, 0x82, 0x51, 0x93, 0xFA, 0x82, 0x51,
                         0x3A, 0x3C, 0x82, 0x51, 0x81, 0x80, 0x81, 0x8E,
                         0x82, 0x51];

  test_decodePDUMsg(
      testDataBuffer1,
      "\uFF19\u0033\u0041\u0061\u0033\uFF21\uFF41\u0033\uFF71\uFF6F\u0033\u30A2\u30F6\u0033\u3042\u0033\u65E5\u0033\u003A\u003C\u0033\u00F7\u2103\u0033\u0031\uFF12\u0041\u0061\uFF12\uFF21\uFF41\uFF12\uFF71\uFF6F\uFF12\u30A2\u30F6\uFF12\u3042\uFF12\u65E5\uFF12\u003A\u003C\uFF12\u00F7\u2103\uFF12",
      PDU_CDMA_MSG_CODING_SHIFT_JIS,
      undefined,
      testDataBuffer1.length
  );

  // test case 2
  let testDataBuffer2 = [0x31, 0x51, 0x63, 0x82, 0x58, 0x51, 0x63, 0x82,
                         0x60, 0x82, 0x81, 0x51, 0x63, 0xB1, 0xAF, 0x51,
                         0x63, 0x83, 0x41, 0x83, 0x96, 0x51, 0x63, 0x82,
                         0xA0, 0x51, 0x63, 0x93, 0xFA, 0x51, 0x63, 0x3A,
                         0x3C, 0x51, 0x63, 0x81, 0x80, 0x81, 0x8E, 0x51,
                         0x63, 0x31, 0x82, 0x70, 0x82, 0x85, 0x82, 0x58,
                         0x82, 0x70, 0x82, 0x85, 0x41, 0x61, 0x82, 0x70,
                         0x82, 0x85, 0xB1, 0xAF, 0x82, 0x70, 0x82, 0x85,
                         0x83, 0x41, 0x83, 0x96, 0x82, 0x70, 0x82, 0x85,
                         0x82, 0xA0, 0x82, 0x70, 0x82, 0x85, 0x93, 0xFA,
                         0x82, 0x70, 0x82, 0x85, 0x3A, 0x3C, 0x82, 0x70,
                         0x82, 0x85, 0x81, 0x80, 0x81, 0x8E, 0x82, 0x70,
                         0x82, 0x85];

  test_decodePDUMsg(
      testDataBuffer2,
      "\u0031\u0051\u0063\uFF19\u0051\u0063\uFF21\uFF41\u0051\u0063\uFF71\uFF6F\u0051\u0063\u30A2\u30F6\u0051\u0063\u3042\u0051\u0063\u65E5\u0051\u0063\u003A\u003C\u0051\u0063\u00F7\u2103\u0051\u0063\u0031\uFF31\uFF45\uFF19\uFF31\uFF45\u0041\u0061\uFF31\uFF45\uFF71\uFF6F\uFF31\uFF45\u30A2\u30F6\uFF31\uFF45\u3042\uFF31\uFF45\u65E5\uFF31\uFF45\u003A\u003C\uFF31\uFF45\u00F7\u2103\uFF31\uFF45",
      PDU_CDMA_MSG_CODING_SHIFT_JIS,
      undefined,
      testDataBuffer2.length
  );

  // test case 3
  let testDataBuffer3 = [0x31, 0xC2, 0xDF, 0x82, 0x58, 0xC2, 0xDF, 0x41,
                         0x61, 0xC2, 0xDF, 0x82, 0x60, 0x82, 0x81, 0xC2,
                         0xDF, 0x83, 0x41, 0x83, 0x96, 0xC2, 0xDF, 0x82,
                         0xA0, 0xC2, 0xDF, 0x93, 0xFA, 0xC2, 0xDF, 0x3A,
                         0x3C, 0xC2, 0xDF, 0x81, 0x80, 0x81, 0x8E, 0xC2,
                         0xDF, 0x31, 0x83, 0x51, 0x83, 0x87, 0x82, 0x58,
                         0x83, 0x51, 0x83, 0x87, 0x41, 0x61, 0x83, 0x51,
                         0x83, 0x87, 0x82, 0x60, 0x82, 0x81, 0x83, 0x51,
                         0x83, 0x87, 0xB1, 0xAF, 0x83, 0x51, 0x83, 0x87,
                         0x82, 0xA0, 0x83, 0x51, 0x83, 0x87, 0x93, 0xFA,
                         0x83, 0x51, 0x83, 0x87, 0x3A, 0x3C, 0x83, 0x51,
                         0x83, 0x87, 0x81, 0x80, 0x81, 0x8E, 0x83, 0x51,
                         0x83, 0x87];

  test_decodePDUMsg(
      testDataBuffer3,
      "\u0031\uFF82\uFF9F\uFF19\uFF82\uFF9F\u0041\u0061\uFF82\uFF9F\uFF21\uFF41\uFF82\uFF9F\u30A2\u30F6\uFF82\uFF9F\u3042\uFF82\uFF9F\u65E5\uFF82\uFF9F\u003A\u003C\uFF82\uFF9F\u00F7\u2103\uFF82\uFF9F\u0031\u30B2\u30E7\uFF19\u30B2\u30E7\u0041\u0061\u30B2\u30E7\uFF21\uFF41\u30B2\u30E7\uFF71\uFF6F\u30B2\u30E7\u3042\u30B2\u30E7\u65E5\u30B2\u30E7\u003A\u003C\u30B2\u30E7\u00F7\u2103\u30B2\u30E7",
      PDU_CDMA_MSG_CODING_SHIFT_JIS,
      undefined,
      testDataBuffer3.length
  );

  // test case 4
  let testDataBuffer4 = [0x31, 0x82, 0xB0, 0x82, 0x58, 0x82, 0xB0, 0x41,
                         0x61, 0x82, 0xB0, 0x82, 0x60, 0x82, 0x81, 0x82,
                         0xB0, 0xB1, 0xAF, 0x82, 0xB0, 0x83, 0x41, 0x83,
                         0x96, 0x82, 0xB0, 0x93, 0xFA, 0x82, 0xB0, 0x3A,
                         0x3C, 0x82, 0xB0, 0x81, 0x80, 0x81, 0x8E, 0x82,
                         0xB0, 0x31, 0x88, 0xA4, 0x82, 0x58, 0x88, 0xA4,
                         0x41, 0x61, 0x88, 0xA4, 0x82, 0x60, 0x82, 0x81,
                         0x88, 0xA4, 0xB1, 0xAF, 0x88, 0xA4, 0x83, 0x41,
                         0x83, 0x96, 0x88, 0xA4, 0x82, 0xA0, 0x88, 0xA4,
                         0x3A, 0x3C, 0x88, 0xA4, 0x81, 0x80, 0x81, 0x8E,
                         0x88, 0xA4];

  test_decodePDUMsg(
      testDataBuffer4,
      "\u0031\u3052\uFF19\u3052\u0041\u0061\u3052\uFF21\uFF41\u3052\uFF71\uFF6F\u3052\u30A2\u30F6\u3052\u65E5\u3052\u003A\u003C\u3052\u00F7\u2103\u3052\u0031\u611B\uFF19\u611B\u0041\u0061\u611B\uFF21\uFF41\u611B\uFF71\uFF6F\u611B\u30A2\u30F6\u611B\u3042\u611B\u003A\u003C\u611B\u00F7\u2103\u611B",
      PDU_CDMA_MSG_CODING_SHIFT_JIS,
      undefined,
      testDataBuffer4.length
  );

  // test case 5
  let testDataBuffer5 = [0x31, 0x40, 0x82, 0x58, 0x40, 0x41, 0x61, 0x40,
                         0x82, 0x60, 0x82, 0x81, 0x40, 0xB1, 0xAF, 0x40,
                         0x83, 0x41, 0x83, 0x96, 0x40, 0x82, 0xA0, 0x40,
                         0x93, 0xFA, 0x40, 0x81, 0x80, 0x81, 0x8E, 0x40,
                         0x31, 0x81, 0x9B, 0x82, 0x58, 0x81, 0x9B, 0x41,
                         0x61, 0x81, 0x9B, 0x82, 0x60, 0x82, 0x81, 0x81,
                         0x9B, 0xB1, 0xAF, 0x81, 0x9B, 0x83, 0x41, 0x83,
                         0x96, 0x81, 0x9B, 0x82, 0xA0, 0x81, 0x9B, 0x93,
                         0xFA, 0x81, 0x9B, 0x3A, 0x3C, 0x81, 0x9B];

  test_decodePDUMsg(
      testDataBuffer5,
      "\u0031\u0040\uFF19\u0040\u0041\u0061\u0040\uFF21\uFF41\u0040\uFF71\uFF6F\u0040\u30A2\u30F6\u0040\u3042\u0040\u65E5\u0040\u00F7\u2103\u0040\u0031\u25CB\uFF19\u25CB\u0041\u0061\u25CB\uFF21\uFF41\u25CB\uFF71\uFF6F\u25CB\u30A2\u30F6\u25CB\u3042\u25CB\u65E5\u25CB\u003A\u003C\u25CB",
      PDU_CDMA_MSG_CODING_SHIFT_JIS,
      undefined,
      testDataBuffer5.length
  );

  run_next_test();
});
