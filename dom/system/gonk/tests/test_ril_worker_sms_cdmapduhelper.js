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
    postRILMessage: function fakePostRILMessage(data) {
      // Do nothing
    },
    postMessage: function fakePostMessage(message) {
      // Do nothing
    }
  });

  let testDataBuffer = [];
  worker.BitBufferHelper.startWrite(testDataBuffer);

  let helper = worker.CdmaPDUHelper;
  helper.encodeUserDataReplyOption({requestStatusReport: true});

  let expectedDataBuffer = [PDU_CDMA_MSG_USERDATA_REPLY_OPTION, 0x01, 0x40];

  do_check_eq(testDataBuffer.length, expectedDataBuffer.length);

  for (let i = 0; i < expectedDataBuffer.length; i++) {
    do_check_eq(testDataBuffer[i], expectedDataBuffer[i]);
  }

  run_next_test();
});

/**
 * Verify CdmaPDUHelper#cdma_decodeUserDataMsgStatus.
 */
add_test(function test_CdmaPDUHelper_decodeUserDataMsgStatus() {
  let worker = newWorker({
    postRILMessage: function fakePostRILMessage(data) {
      // Do nothing
    },
    postMessage: function fakePostMessage(message) {
      // Do nothing
    }
  });

  let helper = worker.CdmaPDUHelper;
  function test_MsgStatus(octet) {
    let testDataBuffer = [octet];
    worker.BitBufferHelper.startRead(testDataBuffer);
    let result = helper.decodeUserDataMsgStatus();

    do_check_eq(result.errorClass, octet >>> 6);
    do_check_eq(result.msgStatus, octet & 0x3F);
  }

  // 00|000000: no error|Message accepted
  test_MsgStatus(0x00);

  // 10|000100: temporary condition|Network congestion
  test_MsgStatus(0x84);

  // 11|000101: permanent condition|Network error
  test_MsgStatus(0xC5);

  run_next_test();
});
