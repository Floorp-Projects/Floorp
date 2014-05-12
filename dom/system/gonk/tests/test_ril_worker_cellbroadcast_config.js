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
  let context = worker.ContextPool._contexts[0];

  let parcelTypes = [];
  let org_newParcel = context.Buf.newParcel;
  context.Buf.newParcel = function(type, options) {
    parcelTypes.push(type);
    org_newParcel.apply(this, arguments);
  };

  function setup(isCdma) {
    context.RIL._isCdma = isCdma;
    context.RIL.cellBroadcastDisabled = false;
    context.RIL.mergedCellBroadcastConfig = [1, 2, 4, 7];  // 1, 4-6
    parcelTypes = [];
  }

  function test(isCdma, expectedRequest) {
    setup(isCdma);
    context.RIL.setCellBroadcastDisabled({disabled: true});
    // Makesure that request parcel is sent out.
    notEqual(parcelTypes.indexOf(expectedRequest), -1);
    equal(context.RIL.cellBroadcastDisabled, true);
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
  let context = worker.ContextPool._contexts[0];

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
      let u32Parcel = U32ArrayFromParcelArray(Array.slice(parcel));
      if (u32Parcel[1] != parcelType) {
        return;
      }

      found = true;
      // Check parcel. Data start from 4th word (32bit)
      equal(u32Parcel.slice(3).toString(), expected);
    };

    context.RIL._isCdma = isCdma;
    context.RIL.setSmsBroadcastConfig(configs);

    // Makesure that request parcel is sent out.
    ok(found);
  }

  // (GSM) RIL writes the following data to outgoing parcel:
  //   nums [(from, to, 0, 0xFF, 1), ... ]
  test(false,
       [1, 2, 4, 7]  /* 1, 4-6 */,
       ["2", "1,1,0,255,1", "4,6,0,255,1"].join());

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
  let context = worker.ContextPool._contexts[0];

  function test(isCdma, configs, expected) {
    context.RIL._isCdma = isCdma;
    context.RIL.cellBroadcastConfigs = configs;
    context.RIL._mergeAllCellBroadcastConfigs();
    equal(context.RIL.mergedCellBroadcastConfig.toString(), expected);
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

add_test(function test_ril_worker_cellbroadcast_set_search_list() {
  let worker = newWorker({
    postRILMessage: function(id, parcel) {
      // Do nothing
    },
    postMessage: function(message) {
      // Do nothing
    }
  });

  let context = worker.ContextPool._contexts[0];

  function test(aIsCdma, aSearchList, aExpected) {
    context.RIL._isCdma = aIsCdma;

    let options = { searchList: aSearchList };
    context.RIL.setCellBroadcastSearchList(options);
    // Enforce the MMI result to string for comparison.
    equal("" + context.RIL.cellBroadcastConfigs.MMI, aExpected);
    do_check_eq(options.errorMsg, undefined);
  }

  let searchListStr = "1,2,3,4";
  let searchList = { gsm: "1,2,3,4", cdma: "5,6,7,8" };

  test(false, searchListStr, "1,2,2,3,3,4,4,5");
  test(true, searchListStr, "1,2,2,3,3,4,4,5");
  test(false, searchList, "1,2,2,3,3,4,4,5");
  test(true, searchList, "5,6,6,7,7,8,8,9");
  test(false, null, "null");
  test(true, null, "null");

  run_next_test();
});
