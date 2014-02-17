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
  let record = helper.decodeInformationRecord();

  do_check_eq(record.display, "Test Info");

  run_next_test();
});

/**
 * Verify decoder for type EXTENDED DISPLAY
 */
add_test(function test_extended_display() {
  let worker = newWorkerWithParcel([
                0x01, // one inforemation record
                0x07, // type: extended display
                0x0E, // length: 14
                0x80, // header byte
                0x80, // Blank
                0x81, // Skip
                0x9B, // Text
                0x09, 0x54, 0x65, 0x73, 0x74, 0x20, 0x49, 0x6E,
                0x66, 0x6F, 0x00]);
  let context = worker.ContextPool._contexts[0];
  let helper = context.CdmaPDUHelper;
  let record = helper.decodeInformationRecord();

  do_check_eq(record.extendedDisplay.indicator, 1);
  do_check_eq(record.extendedDisplay.type, 0);
  do_check_eq(record.extendedDisplay.records.length, 3);
  do_check_eq(record.extendedDisplay.records[0].tag, 0x80);
  do_check_eq(record.extendedDisplay.records[1].tag, 0x81);
  do_check_eq(record.extendedDisplay.records[2].tag, 0x9B);
  do_check_eq(record.extendedDisplay.records[2].content, "Test Info");

  run_next_test();
});
/**
 * Verify decoder for mixed type
 */
add_test(function test_mixed() {
  let worker = newWorkerWithParcel([
                0x02, // two inforemation record
                0x00, // type: display
                0x09, // length: 9
                0x54, 0x65, 0x73, 0x74, 0x20, 0x49, 0x6E, 0x66,
                0x6F, 0x00,
                0x07, // type: extended display
                0x0E, // length: 14
                0x80, // header byte
                0x80, // Blank
                0x81, // Skip
                0x9B, // Text
                0x09, 0x54, 0x65, 0x73, 0x74, 0x20, 0x49, 0x6E,
                0x66, 0x6F, 0x00]);
  let context = worker.ContextPool._contexts[0];
  let helper = context.CdmaPDUHelper;
  let record = helper.decodeInformationRecord();

  do_check_eq(record.display, "Test Info");
  do_check_eq(record.extendedDisplay.indicator, 1);
  do_check_eq(record.extendedDisplay.type, 0);
  do_check_eq(record.extendedDisplay.records.length, 3);
  do_check_eq(record.extendedDisplay.records[0].tag, 0x80);
  do_check_eq(record.extendedDisplay.records[1].tag, 0x81);
  do_check_eq(record.extendedDisplay.records[2].tag, 0x9B);
  do_check_eq(record.extendedDisplay.records[2].content, "Test Info");

  run_next_test();
});
