/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  run_next_test();
}

/**
 * Add test function with specified parcel and request handler.
 *
 * @param parcel
 *        Incoming parcel to be tested.
 * @param handler
 *        Handler to be invoked as RIL request handler.
 */
function add_test_incoming_parcel(parcel, handler) {
  add_test(function test_incoming_parcel() {
    let worker = newWorker({
      postRILMessage: function(data) {
        // do nothing
      },
      postMessage: function(message) {
        // do nothing
      }
    });

    if (!parcel) {
      parcel = newIncomingParcel(-1,
                                 worker.RESPONSE_TYPE_UNSOLICITED,
                                 worker.REQUEST_VOICE_REGISTRATION_STATE,
                                 [0, 0, 0, 0]);
    }

    // supports only requests less or equal than UINT8_MAX(255).
    let buf = worker.Buf;
    let request = parcel[buf.PARCEL_SIZE_SIZE + buf.UINT32_SIZE];
    worker.RIL[request] = function ril_request_handler() {
      handler(worker);
      worker.postMessage();
    };

    worker.onRILMessage(parcel);

    // end of incoming parcel's trip, let's do next test.
    run_next_test();
  });
}

// Test normal parcel handling.
add_test_incoming_parcel(null,
  function test_normal_parcel_handling(worker) {
    do_check_throws(function normal_handler() {
      // reads exactly the same size, should not throw anything.
      worker.Buf.readInt32();
    });
  }
);

// Test parcel under read.
add_test_incoming_parcel(null,
  function test_parcel_under_read(worker) {
    do_check_throws(function under_read_handler() {
      // reads less than parcel size, should not throw.
      worker.Buf.readUint16();
    });
  }
);

// Test parcel over read.
add_test_incoming_parcel(null,
  function test_parcel_over_read(worker) {
    let buf = worker.Buf;

    // read all data available
    while (buf.readAvailable > 0) {
      buf.readUint8();
    }

    do_check_throws(function over_read_handler() {
      // reads more than parcel size, should throw an error.
      buf.readUint8();
    },"Trying to read data beyond the parcel end!");
  }
);

// Test Bug 814761: buffer overwritten
add_test(function test_incoming_parcel_buffer_overwritten() {
  let worker = newWorker({
    postRILMessage: function(data) {
      // do nothing
    },
    postMessage: function(message) {
      // do nothing
    }
  });

  // A convenient alias.
  let buf = worker.Buf;

  // Allocate an array of specified size and set each of its elements to value.
  function calloc(length, value) {
    let array = new Array(length);
    for (let i = 0; i < length; i++) {
      array[i] = value;
    }
    return array;
  }

  // Do nothing in handleParcel().
  let request = worker.REQUEST_VOICE_REGISTRATION_STATE;
  worker.RIL[request] = null;

  // Prepare two parcels, whose sizes are both smaller than the incoming buffer
  // size but larger when combined, to trigger the bug.
  let pA_dataLength = buf.incomingBufferLength / 2;
  let pA = newIncomingParcel(-1,
                             worker.RESPONSE_TYPE_UNSOLICITED,
                             request,
                             calloc(pA_dataLength, 1));
  let pA_parcelSize = pA.length - buf.PARCEL_SIZE_SIZE;

  let pB_dataLength = buf.incomingBufferLength * 3 / 4;
  let pB = newIncomingParcel(-1,
                             worker.RESPONSE_TYPE_UNSOLICITED,
                             request,
                             calloc(pB_dataLength, 1));
  let pB_parcelSize = pB.length - buf.PARCEL_SIZE_SIZE;

  // First, send an incomplete pA and verifies related data pointer:
  let p1 = pA.subarray(0, pA.length - 1);
  worker.onRILMessage(p1);
  // The parcel should not have been processed.
  do_check_eq(buf.readAvailable, 0);
  // buf.currentParcelSize should have been set because incoming data has more
  // than 4 octets.
  do_check_eq(buf.currentParcelSize, pA_parcelSize);
  // buf.readIncoming should contains remaining unconsumed octets count.
  do_check_eq(buf.readIncoming, p1.length - buf.PARCEL_SIZE_SIZE);
  // buf.incomingWriteIndex should be ready to accept the last octet.
  do_check_eq(buf.incomingWriteIndex, p1.length);

  // Second, send the last octet of pA and whole pB. The Buf should now expand
  // to cover both pA & pB.
  let p2 = new Uint8Array(1 + pB.length);
  p2.set(pA.subarray(pA.length - 1), 0);
  p2.set(pB, 1);
  worker.onRILMessage(p2);
  // The parcels should have been both consumed.
  do_check_eq(buf.readAvailable, 0);
  // No parcel data remains.
  do_check_eq(buf.currentParcelSize, 0);
  // No parcel data remains.
  do_check_eq(buf.readIncoming, 0);
  // The Buf should now expand to cover both pA & pB.
  do_check_eq(buf.incomingWriteIndex, pA.length + pB.length);

  // end of incoming parcel's trip, let's do next test.
  run_next_test();
});

// Test Buf.readUint8Array.
add_test_incoming_parcel(null,
  function test_buf_readUint8Array(worker) {
    let buf = worker.Buf;

    let u8array = buf.readUint8Array(1);
    do_check_eq(u8array instanceof Uint8Array, true);
    do_check_eq(u8array.length, 1);
    do_check_eq(buf.readAvailable, 3);

    u8array = buf.readUint8Array(2);
    do_check_eq(u8array.length, 2);
    do_check_eq(buf.readAvailable, 1);

    do_check_throws(function over_read_handler() {
      // reads more than parcel size, should throw an error.
      u8array = buf.readUint8Array(2);
    }, "Trying to read data beyond the parcel end!");
  }
);
