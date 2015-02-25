/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

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
    ok(false);
    run_next_test();
  };

  ril.iccIO = function fakeIccIO(options) {
    ok(true);
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
    ok(true);
    run_next_test();
  };

  ril.iccIO = function fakeIccIO(options) {
    // When recordSize is not provided, loadLinearFixedEF should call getResponse.
    ok(false);
    run_next_test();
  };

  io.loadLinearFixedEF({});
});

/**
 * Verify ICC IO Error.
 */
add_test(function test_process_icc_io_error() {
  let worker = newUint8Worker();
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;

  function do_test(sw1, sw2, expectedErrorMsg) {
    let called = false;
    function errorCb(errorMsg) {
      called = true;
      equal(errorMsg, expectedErrorMsg);
    }

    // Write sw1 and sw2 to buffer.
    buf.writeInt32(sw1);
    buf.writeInt32(sw2);

    context.RIL[REQUEST_SIM_IO](0, {fileId: 0xffff,
                                    command: 0xff,
                                    onerror: errorCb});

    // onerror callback should be triggered.
    ok(called);
  }

  let TEST_DATA = [
    // [sw1, sw2, expectError]
    [ICC_STATUS_ERROR_COMMAND_NOT_ALLOWED, 0xff, GECKO_ERROR_GENERIC_FAILURE],
    [ICC_STATUS_ERROR_WRONG_PARAMETERS, 0xff, GECKO_ERROR_GENERIC_FAILURE],
  ];

  for (let i = 0; i < TEST_DATA.length; i++) {
    do_test.apply(null, TEST_DATA[i]);
  }

  run_next_test();
});

/**
 * Verify ICCIOHelper.processICCIOGetResponse for EF_TYPE_TRANSPARENT.
 */
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
                   structure: EF_STRUCTURE_TRANSPARENT};
    iccioHelper.processICCIOGetResponse(options);

    equal(options.fileSize, 0x0A);
  }

  run_next_test();
});

/**
 * Verify ICCIOHelper.processICCIOGetResponse for EF_TYPE_LINEAR_FIXED.
 */
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
                   structure: EF_STRUCTURE_LINEAR_FIXED};
    iccioHelper.processICCIOGetResponse(options);

    equal(options.fileSize, 0x1A);
    equal(options.recordSize, 0x1A);
    equal(options.totalRecords, 0x01);
  }

  run_next_test();
});

