/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

/**
 * Helper function.
 */
function newWorkerWithParcel(parcelBuf) {
  let worker = newWorker({
    postRILMessage: function(data) {
      // Do nothing
    },
    postMessage: function(message) {
      // Do nothing
    }
  });

  let index = 0; // index for read
  let buf = parcelBuf;

  let context = worker.ContextPool._contexts[0];
  context.Buf.readUint8 = function() {
    return buf[index++];
  };

  context.Buf.readUint16 = function() {
    return buf[index++];
  };

  context.Buf.readInt32 = function() {
    return buf[index++];
  };

  context.Buf.seekIncoming = function(offset) {
    index += offset / context.Buf.UINT32_SIZE;
  };

  return worker;
}

// Test CDMA information record decoder.

/**
 * Verify decoder for type DISPLAY
 */
add_test(function test_display() {
  let worker = newWorkerWithParcel([
                0x01, // one inforemation record
                0x00, // type: display
                0x09, // length: 9
                0x54, 0x65, 0x73, 0x74, 0x20, 0x49, 0x6E, 0x66,
                0x6F, 0x00]);
  let context = worker.ContextPool._contexts[0];
  let helper = context.CdmaPDUHelper;
  let records = helper.decodeInformationRecord();

  equal(records[0].display, "Test Info");

  run_next_test();
});

/**
 * Verify decoder for type EXTENDED DISPLAY
 */
add_test(function test_extended_display() {
  let worker = newWorkerWithParcel([
                0x01, // one inforemation record
                0x07, // type: extended display
                0x12, // length: 18
                0x54, 0x65, 0x73, 0x74, 0x20, 0x45, 0x78, 0x74,
                0x65, 0x6E, 0x64, 0x65, 0x64, 0x20, 0x49, 0x6E,
                0x66, 0x6F, 0x00, 0x00]);
  let context = worker.ContextPool._contexts[0];
  let helper = context.CdmaPDUHelper;
  let records = helper.decodeInformationRecord();

  equal(records[0].display, "Test Extended Info");

  run_next_test();
});

/**
 * Verify decoder for mixed type
 */
add_test(function test_mixed() {
  let worker = newWorkerWithParcel([
                0x02, // two inforemation record
                0x00, // type: display
                0x0B, // length: 11
                0x54, 0x65, 0x73, 0x74, 0x20, 0x49, 0x6E, 0x66,
                0x6F, 0x20, 0x31, 0x00,
                0x07, // type: extended display
                0x0B, // length: 11
                0x54, 0x65, 0x73, 0x74, 0x20, 0x49, 0x6E, 0x66,
                0x6F, 0x20, 0x32, 0x00]);
  let context = worker.ContextPool._contexts[0];
  let helper = context.CdmaPDUHelper;
  let records = helper.decodeInformationRecord();

  equal(records[0].display, "Test Info 1");
  equal(records[1].display, "Test Info 2");

  run_next_test();
});

/**
 * Verify decoder for multiple types
 */
add_test(function test_multiple() {
  let worker = newWorkerWithParcel([
                0x02, // two inforemation record
                0x00, // type: display
                0x0B, // length: 11
                0x54, 0x65, 0x73, 0x74, 0x20, 0x49, 0x6E, 0x66,
                0x6F, 0x20, 0x31, 0x00,
                0x00, // type: display
                0x0B, // length: 11
                0x54, 0x65, 0x73, 0x74, 0x20, 0x49, 0x6E, 0x66,
                0x6F, 0x20, 0x32, 0x00]);
  let context = worker.ContextPool._contexts[0];
  let helper = context.CdmaPDUHelper;
  let records = helper.decodeInformationRecord();

  equal(records[0].display, "Test Info 1");
  equal(records[1].display, "Test Info 2");

  run_next_test();
});

/**
 * Verify decoder for Signal Type
 */
add_test(function test_signal() {
  let worker = newWorkerWithParcel([
                0x01,   // one inforemation record
                0x04,   // type: signal
                0x01,   // isPresent: non-zero
                0x00,   // signalType: Tone signal (00)
                0x01,   // alertPitch: High pitch
                0x03]); // signal: Abbreviated intercept (000011)
  let context = worker.ContextPool._contexts[0];
  let helper = context.CdmaPDUHelper;
  let records = helper.decodeInformationRecord();

  equal(records[0].signal.type, 0x00);
  equal(records[0].signal.alertPitch, 0x01);
  equal(records[0].signal.signal, 0x03);

  run_next_test();
});

/**
 * Verify decoder for Signal Type for Not Presented
 */
add_test(function test_signal_not_present() {
  let worker = newWorkerWithParcel([
                0x01,   // one inforemation record
                0x04,   // type: signal
                0x00,   // isPresent: zero
                0x00,   // signalType: Tone signal (00)
                0x01,   // alertPitch: High pitch
                0x03]); // signal: Abbreviated intercept (000011)
  let context = worker.ContextPool._contexts[0];
  let helper = context.CdmaPDUHelper;
  let records = helper.decodeInformationRecord();

  equal(records.length, 0);

  run_next_test();
});

/**
 * Verify decoder for Line Control
 */
add_test(function test_line_control() {
  let worker = newWorkerWithParcel([
                0x01,   // one inforemation record
                0x06,   // type: line control
                0x01,   // polarity included
                0x00,   // not toggled
                0x01,   // reversed
                0xFF]); // Power denial timeout: 255 * 5 ms
  let context = worker.ContextPool._contexts[0];
  let helper = context.CdmaPDUHelper;
  let records = helper.decodeInformationRecord();

  equal(records[0].lineControl.polarityIncluded, 1);
  equal(records[0].lineControl.toggle, 0);
  equal(records[0].lineControl.reverse, 1);
  equal(records[0].lineControl.powerDenial, 255);

  run_next_test();
});

/**
 * Verify decoder for CLIR Cause
 */
add_test(function test_clir() {
  let worker = newWorkerWithParcel([
                0x01,   // one inforemation record
                0x08,   // type: clir
                0x01]); // cause: Rejected by user
  let context = worker.ContextPool._contexts[0];
  let helper = context.CdmaPDUHelper;
  let records = helper.decodeInformationRecord();

  equal(records[0].clirCause, 1);

  run_next_test();
});

/**
 * Verify decoder for Audio Control
 */
add_test(function test_clir() {
  let worker = newWorkerWithParcel([
                0x01,   // one inforemation record
                0x0A,   // type: audio control
                0x01,   // uplink
                0xFF]); // downlink
  let context = worker.ContextPool._contexts[0];
  let helper = context.CdmaPDUHelper;
  let records = helper.decodeInformationRecord();

  equal(records[0].audioControl.upLink, 1);
  equal(records[0].audioControl.downLink, 255);

  run_next_test();
});
