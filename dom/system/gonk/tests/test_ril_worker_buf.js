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

    let context = worker.ContextPool._contexts[0];
    // supports only requests less or equal than UINT8_MAX(255).
    let buf = context.Buf;
    let request = parcel[buf.PARCEL_SIZE_SIZE + buf.UINT32_SIZE];
    context.RIL[request] = function ril_request_handler() {
      handler.apply(this, arguments);
    };

    worker.onRILMessage(0, parcel);

    // end of incoming parcel's trip, let's do next test.
    run_next_test();
  });
}

// Test normal parcel handling.
add_test_incoming_parcel(null,
  function test_normal_parcel_handling() {
    let self = this;
    try {
      // reads exactly the same size, should not throw anything.
      self.context.Buf.readInt32();
    } catch (e) {
      ok(false, "Got exception: " + e);
    }
  }
);

// Test parcel under read.
add_test_incoming_parcel(null,
  function test_parcel_under_read() {
    let self = this;
    try {
      // reads less than parcel size, should not throw.
      self.context.Buf.readUint16();
    } catch (e) {
      ok(false, "Got exception: " + e);
    }
  }
);

// Test parcel over read.
add_test_incoming_parcel(null,
  function test_parcel_over_read() {
    let buf = this.context.Buf;

    // read all data available
    while (buf.readAvailable > 0) {
      buf.readUint8();
    }

    throws(function over_read_handler() {
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

  let context = worker.ContextPool._contexts[0];
  // A convenient alias.
  let buf = context.Buf;

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
  context.RIL[request] = null;

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
  worker.onRILMessage(0, p1);
  // The parcel should not have been processed.
  equal(buf.readAvailable, 0);
  // buf.currentParcelSize should have been set because incoming data has more
  // than 4 octets.
  equal(buf.currentParcelSize, pA_parcelSize);
  // buf.readIncoming should contains remaining unconsumed octets count.
  equal(buf.readIncoming, p1.length - buf.PARCEL_SIZE_SIZE);
  // buf.incomingWriteIndex should be ready to accept the last octet.
  equal(buf.incomingWriteIndex, p1.length);

  // Second, send the last octet of pA and whole pB. The Buf should now expand
  // to cover both pA & pB.
  let p2 = new Uint8Array(1 + pB.length);
  p2.set(pA.subarray(pA.length - 1), 0);
  p2.set(pB, 1);
  worker.onRILMessage(0, p2);
  // The parcels should have been both consumed.
  equal(buf.readAvailable, 0);
  // No parcel data remains.
  equal(buf.currentParcelSize, 0);
  // No parcel data remains.
  equal(buf.readIncoming, 0);
  // The Buf should now expand to cover both pA & pB.
  equal(buf.incomingWriteIndex, pA.length + pB.length);

  // end of incoming parcel's trip, let's do next test.
  run_next_test();
});

// Test Buf.readUint8Array.
add_test_incoming_parcel(null,
  function test_buf_readUint8Array() {
    let buf = this.context.Buf;

    let u8array = buf.readUint8Array(1);
    equal(u8array instanceof Uint8Array, true);
    equal(u8array.length, 1);
    equal(buf.readAvailable, 3);

    u8array = buf.readUint8Array(2);
    equal(u8array.length, 2);
    equal(buf.readAvailable, 1);

    throws(function over_read_handler() {
      // reads more than parcel size, should throw an error.
      u8array = buf.readUint8Array(2);
    }, "Trying to read data beyond the parcel end!");
  }
);
