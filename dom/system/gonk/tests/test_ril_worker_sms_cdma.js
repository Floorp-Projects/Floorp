/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

function _getWorker() {
  let _postedMessage;
  let _worker = newWorker({
    postRILMessage: function fakePostRILMessage(data) {
    },
    postMessage: function fakePostMessage(message) {
      _postedMessage = message;
    }
  });
  return {
    get postedMessage() {
      return _postedMessage;
    },
    get worker() {
      return _worker;
    }
  };
}

/**
 * Verify CDMA SMS Delivery ACK Message.
 */
add_test(function test_processCdmaSmsStatusReport() {
  let workerHelper = _getWorker();
  let worker = workerHelper.worker;

  function test_StatusReport(errorClass, msgStatus) {
    let msgId = 0;
    let sentSmsMap = worker.RIL._pendingSentSmsMap;

    sentSmsMap[msgId] = {};

    let message = {
      SMSC:             "",
      mti:              0,
      udhi:             0,
      sender:           "0987654321",
      recipient:        null,
      pid:              PDU_PID_DEFAULT,
      epid:             PDU_PID_DEFAULT,
      dcs:              0,
      mwi:              null,
      replace:          false,
      header:           null,
      body:             "Status: Sent, Dest: 0987654321",
      data:             null,
      timestamp:        new Date().valueOf(),
      language:         null,
      status:           null,
      scts:             null,
      dt:               null,
      encoding:         PDU_CDMA_MSG_CODING_7BITS_ASCII,
      messageClass:     GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
      messageType:      PDU_CDMA_MSG_TYPE_P2P,
      serviceCategory:  0,
      subMsgType:       PDU_CDMA_MSG_TYPE_DELIVER_ACK,
      msgId:            msgId,
      errorClass:       errorClass,
      msgStatus:        msgStatus
    };

    worker.RIL._processCdmaSmsStatusReport(message);

    let postedMessage = workerHelper.postedMessage;

    // Check if pending token is removed.
    do_check_true((errorClass === 2)? !!sentSmsMap[msgId]: !sentSmsMap[msgId]);

    // Check the response message accordingly.
    if (errorClass === -1) {
      // Check if the report is treated as normal incoming SMS
      do_check_eq("sms-received", postedMessage.rilMessageType);
    } else if (errorClass === 2) {
      // Do nothing.
    } else {
      // Check Delivery Status
      if (errorClass === 0) {
        do_check_eq(postedMessage.deliveryStatus, GECKO_SMS_DELIVERY_STATUS_SUCCESS);
      } else {
        do_check_eq(postedMessage.deliveryStatus, GECKO_SMS_DELIVERY_STATUS_ERROR);
      }
    }
  }

  test_StatusReport(-1, -1); // Message Status Sub-parameter is absent.
  test_StatusReport(0, 0);   // 00|000000: no error|Message accepted
  test_StatusReport(2, 4);   // 10|000100: temporary condition|Network congestion
  test_StatusReport(3, 5);   // 11|000101: permanent condition|Network error

  run_next_test();
});
