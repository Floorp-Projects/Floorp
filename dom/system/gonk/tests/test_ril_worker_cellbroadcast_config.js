/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

add_test(function test_ril_worker_cellbroadcast_activate() {
  let worker = newWorker({
    postRILMessage: function(id, parcel) {
      // Do nothing
    },
    postMessage: function(message) {
      // Do nothing
    }
  });

  let parcelTypes = [];
  let org_newParcel = worker.Buf.newParcel;
  worker.Buf.newParcel = function(type, options) {
    parcelTypes.push(type);
    org_newParcel.apply(this, arguments);
  };

  function setup(isCdma) {
    worker.RIL._isCdma = isCdma;
    worker.RIL.cellBroadcastDisabled = false;
    worker.RIL.mergedCellBroadcastConfig = [1, 2, 4, 7];  // 1, 4-6
    parcelTypes = [];
  }

  function test(isCdma, expectedRequest) {
    setup(isCdma);
    worker.RIL.setCellBroadcastDisabled({disabled: true});
    // Makesure that request parcel is sent out.
    do_check_neq(parcelTypes.indexOf(expectedRequest), -1);
    do_check_eq(worker.RIL.cellBroadcastDisabled, true);
  }

  test(false, REQUEST_GSM_SMS_BROADCAST_ACTIVATION);
  test(true, REQUEST_CDMA_SMS_BROADCAST_ACTIVATION);

  run_next_test();
});

add_test(function test_ril_worker_cellbroadcast_config() {
  let currentParcel;
  let worker = newWorker({
    postRILMessage: function(id, parcel) {
      currentParcel = parcel;
    },
    postMessage: function(message) {
      // Do nothing
    }
  });

  function U32ArrayFromParcelArray(pa) {
    do_print(pa);
    let out = [];
    for (let i = 0; i < pa.length; i += 4) {
      let data = pa[i] + (pa[i+1] << 8) + (pa[i+2] << 16) + (pa[i+3] << 24);
      out.push(data);
    }
    return out;
  }

  function test(isCdma, configs, expected) {
    let parcelType = isCdma ? REQUEST_CDMA_SET_BROADCAST_SMS_CONFIG
                            : REQUEST_GSM_SET_BROADCAST_SMS_CONFIG;

    let found = false;
    worker.postRILMessage = function(id, parcel) {
      u32Parcel = U32ArrayFromParcelArray(Array.slice(parcel));
      if (u32Parcel[1] != parcelType) {
        return;
      }

      found = true;
      // Check parcel. Data start from 4th word (32bit)
      do_check_eq(u32Parcel.slice(3).toString(), expected);
    };

    worker.RIL._isCdma = isCdma;
    worker.RIL.setSmsBroadcastConfig(configs);

    // Makesure that request parcel is sent out.
    do_check_true(found);
  }

  // (GSM) RIL writes the following data to outgoing parcel:
  //   nums [(from, to, 0, 0xFF, 1), ... ]
  test(false,
       [1, 2, 4, 7]  /* 1, 4-6 */,
       ["2", "1,2,0,255,1", "4,7,0,255,1"].join());

  // (CDMA) RIL writes the following data to outgoing parcel:
  //   nums [(id, 0, 1), ... ]
  test(true,
       [1, 2, 4, 7]  /* 1, 4-6 */,
       ["4", "1,0,1", "4,0,1", "5,0,1", "6,0,1"].join());

  run_next_test();
});

add_test(function test_ril_worker_cellbroadcast_merge_config() {
  let worker = newWorker({
    postRILMessage: function(id, parcel) {
      // Do nothing
    },
    postMessage: function(message) {
      // Do nothing
    }
  });

  function test(isCdma, configs, expected) {
    worker.RIL._isCdma = isCdma;
    worker.RIL.cellBroadcastConfigs = configs;
    worker.RIL._mergeAllCellBroadcastConfigs();
    do_check_eq(worker.RIL.mergedCellBroadcastConfig.toString(), expected);
  }

  let configs = {
    MMI:    [1, 2, 4, 7],   // 1, 4-6
    CBMI:   [6, 9],         // 6-8
    CBMID:  [8, 11],        // 8-10
    CBMIR:  [10, 13]        // 10-12
  };

  test(false, configs, "1,2,4,13");
  test(true, configs, "1,2,4,7");

  run_next_test();
});

