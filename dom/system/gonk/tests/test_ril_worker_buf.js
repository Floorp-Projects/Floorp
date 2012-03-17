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
      postRILMessage: function fakePostRILMessage(data) {
        // do nothing
      },
      postMessage: function fakePostMessage(message) {
        // do nothing
      }
    });

    if (!parcel) {
      parcel = newIncomingParcel(-1,
                                 worker.RESPONSE_TYPE_UNSOLICITED,
                                 worker.REQUEST_REGISTRATION_STATE,
                                 [0, 0, 0, 0]);
    }

    // supports only requests less or equal than UINT8_MAX(255).
    let request = parcel[worker.PARCEL_SIZE_SIZE + worker.UINT32_SIZE];
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
    do_check_throws(function normal_handler(worker) {
      // reads exactly the same size, should not throw anything.
      worker.Buf.readUint32();
    });
  }
);

// Test parcel under read.
add_test_incoming_parcel(null,
  function test_parcel_under_read(worker) {
    do_check_throws(function under_read_handler() {
      // reads less than parcel size, should not throw.
      worker.Buf.readUint16();
    }, false);
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
    }, new Error("Trying to read data beyond the parcel end!").result);
  }
);

