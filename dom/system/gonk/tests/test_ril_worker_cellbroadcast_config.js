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

add_test(function test_ril_worker_mergeCellBroadcastConfigs() {
  let worker = newWorker({
    postRILMessage: function(data) {
      // Do nothing
    },
    postMessage: function(message) {
      // Do nothing
    }
  });

  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;

  function test(olist, from, to, expected) {
    let result = ril._mergeCellBroadcastConfigs(olist, from, to);
    equal(JSON.stringify(expected), JSON.stringify(result));
  }

  test(null, 0, 1, [0, 1]);

  test([10, 13],  7,  8, [ 7,  8, 10, 13]);
  test([10, 13],  7,  9, [ 7,  9, 10, 13]);
  test([10, 13],  7, 10, [ 7, 13]);
  test([10, 13],  7, 11, [ 7, 13]);
  test([10, 13],  7, 12, [ 7, 13]);
  test([10, 13],  7, 13, [ 7, 13]);
  test([10, 13],  7, 14, [ 7, 14]);
  test([10, 13],  7, 15, [ 7, 15]);
  test([10, 13],  7, 16, [ 7, 16]);
  test([10, 13],  8,  9, [ 8,  9, 10, 13]);
  test([10, 13],  8, 10, [ 8, 13]);
  test([10, 13],  8, 11, [ 8, 13]);
  test([10, 13],  8, 12, [ 8, 13]);
  test([10, 13],  8, 13, [ 8, 13]);
  test([10, 13],  8, 14, [ 8, 14]);
  test([10, 13],  8, 15, [ 8, 15]);
  test([10, 13],  8, 16, [ 8, 16]);
  test([10, 13],  9, 10, [ 9, 13]);
  test([10, 13],  9, 11, [ 9, 13]);
  test([10, 13],  9, 12, [ 9, 13]);
  test([10, 13],  9, 13, [ 9, 13]);
  test([10, 13],  9, 14, [ 9, 14]);
  test([10, 13],  9, 15, [ 9, 15]);
  test([10, 13],  9, 16, [ 9, 16]);
  test([10, 13], 10, 11, [10, 13]);
  test([10, 13], 10, 12, [10, 13]);
  test([10, 13], 10, 13, [10, 13]);
  test([10, 13], 10, 14, [10, 14]);
  test([10, 13], 10, 15, [10, 15]);
  test([10, 13], 10, 16, [10, 16]);
  test([10, 13], 11, 12, [10, 13]);
  test([10, 13], 11, 13, [10, 13]);
  test([10, 13], 11, 14, [10, 14]);
  test([10, 13], 11, 15, [10, 15]);
  test([10, 13], 11, 16, [10, 16]);
  test([10, 13], 12, 13, [10, 13]);
  test([10, 13], 12, 14, [10, 14]);
  test([10, 13], 12, 15, [10, 15]);
  test([10, 13], 12, 16, [10, 16]);
  test([10, 13], 13, 14, [10, 14]);
  test([10, 13], 13, 15, [10, 15]);
  test([10, 13], 13, 16, [10, 16]);
  test([10, 13], 14, 15, [10, 13, 14, 15]);
  test([10, 13], 14, 16, [10, 13, 14, 16]);
  test([10, 13], 15, 16, [10, 13, 15, 16]);

  test([10, 13, 14, 17],  7,  8, [ 7,  8, 10, 13, 14, 17]);
  test([10, 13, 14, 17],  7,  9, [ 7,  9, 10, 13, 14, 17]);
  test([10, 13, 14, 17],  7, 10, [ 7, 13, 14, 17]);
  test([10, 13, 14, 17],  7, 11, [ 7, 13, 14, 17]);
  test([10, 13, 14, 17],  7, 12, [ 7, 13, 14, 17]);
  test([10, 13, 14, 17],  7, 13, [ 7, 13, 14, 17]);
  test([10, 13, 14, 17],  7, 14, [ 7, 17]);
  test([10, 13, 14, 17],  7, 15, [ 7, 17]);
  test([10, 13, 14, 17],  7, 16, [ 7, 17]);
  test([10, 13, 14, 17],  7, 17, [ 7, 17]);
  test([10, 13, 14, 17],  7, 18, [ 7, 18]);
  test([10, 13, 14, 17],  7, 19, [ 7, 19]);
  test([10, 13, 14, 17],  8,  9, [ 8,  9, 10, 13, 14, 17]);
  test([10, 13, 14, 17],  8, 10, [ 8, 13, 14, 17]);
  test([10, 13, 14, 17],  8, 11, [ 8, 13, 14, 17]);
  test([10, 13, 14, 17],  8, 12, [ 8, 13, 14, 17]);
  test([10, 13, 14, 17],  8, 13, [ 8, 13, 14, 17]);
  test([10, 13, 14, 17],  8, 14, [ 8, 17]);
  test([10, 13, 14, 17],  8, 15, [ 8, 17]);
  test([10, 13, 14, 17],  8, 16, [ 8, 17]);
  test([10, 13, 14, 17],  8, 17, [ 8, 17]);
  test([10, 13, 14, 17],  8, 18, [ 8, 18]);
  test([10, 13, 14, 17],  8, 19, [ 8, 19]);
  test([10, 13, 14, 17],  9, 10, [ 9, 13, 14, 17]);
  test([10, 13, 14, 17],  9, 11, [ 9, 13, 14, 17]);
  test([10, 13, 14, 17],  9, 12, [ 9, 13, 14, 17]);
  test([10, 13, 14, 17],  9, 13, [ 9, 13, 14, 17]);
  test([10, 13, 14, 17],  9, 14, [ 9, 17]);
  test([10, 13, 14, 17],  9, 15, [ 9, 17]);
  test([10, 13, 14, 17],  9, 16, [ 9, 17]);
  test([10, 13, 14, 17],  9, 17, [ 9, 17]);
  test([10, 13, 14, 17],  9, 18, [ 9, 18]);
  test([10, 13, 14, 17],  9, 19, [ 9, 19]);
  test([10, 13, 14, 17], 10, 11, [10, 13, 14, 17]);
  test([10, 13, 14, 17], 10, 12, [10, 13, 14, 17]);
  test([10, 13, 14, 17], 10, 13, [10, 13, 14, 17]);
  test([10, 13, 14, 17], 10, 14, [10, 17]);
  test([10, 13, 14, 17], 10, 15, [10, 17]);
  test([10, 13, 14, 17], 10, 16, [10, 17]);
  test([10, 13, 14, 17], 10, 17, [10, 17]);
  test([10, 13, 14, 17], 10, 18, [10, 18]);
  test([10, 13, 14, 17], 10, 19, [10, 19]);
  test([10, 13, 14, 17], 11, 12, [10, 13, 14, 17]);
  test([10, 13, 14, 17], 11, 13, [10, 13, 14, 17]);
  test([10, 13, 14, 17], 11, 14, [10, 17]);
  test([10, 13, 14, 17], 11, 15, [10, 17]);
  test([10, 13, 14, 17], 11, 16, [10, 17]);
  test([10, 13, 14, 17], 11, 17, [10, 17]);
  test([10, 13, 14, 17], 11, 18, [10, 18]);
  test([10, 13, 14, 17], 11, 19, [10, 19]);
  test([10, 13, 14, 17], 12, 13, [10, 13, 14, 17]);
  test([10, 13, 14, 17], 12, 14, [10, 17]);
  test([10, 13, 14, 17], 12, 15, [10, 17]);
  test([10, 13, 14, 17], 12, 16, [10, 17]);
  test([10, 13, 14, 17], 12, 17, [10, 17]);
  test([10, 13, 14, 17], 12, 18, [10, 18]);
  test([10, 13, 14, 17], 12, 19, [10, 19]);
  test([10, 13, 14, 17], 13, 14, [10, 17]);
  test([10, 13, 14, 17], 13, 15, [10, 17]);
  test([10, 13, 14, 17], 13, 16, [10, 17]);
  test([10, 13, 14, 17], 13, 17, [10, 17]);
  test([10, 13, 14, 17], 13, 18, [10, 18]);
  test([10, 13, 14, 17], 13, 19, [10, 19]);
  test([10, 13, 14, 17], 14, 15, [10, 13, 14, 17]);
  test([10, 13, 14, 17], 14, 16, [10, 13, 14, 17]);
  test([10, 13, 14, 17], 14, 17, [10, 13, 14, 17]);
  test([10, 13, 14, 17], 14, 18, [10, 13, 14, 18]);
  test([10, 13, 14, 17], 14, 19, [10, 13, 14, 19]);
  test([10, 13, 14, 17], 15, 16, [10, 13, 14, 17]);
  test([10, 13, 14, 17], 15, 17, [10, 13, 14, 17]);
  test([10, 13, 14, 17], 15, 18, [10, 13, 14, 18]);
  test([10, 13, 14, 17], 15, 19, [10, 13, 14, 19]);
  test([10, 13, 14, 17], 16, 17, [10, 13, 14, 17]);
  test([10, 13, 14, 17], 16, 18, [10, 13, 14, 18]);
  test([10, 13, 14, 17], 16, 19, [10, 13, 14, 19]);
  test([10, 13, 14, 17], 17, 18, [10, 13, 14, 18]);
  test([10, 13, 14, 17], 17, 19, [10, 13, 14, 19]);
  test([10, 13, 14, 17], 18, 19, [10, 13, 14, 17, 18, 19]);

  test([10, 13, 16, 19],  7, 14, [ 7, 14, 16, 19]);
  test([10, 13, 16, 19],  7, 15, [ 7, 15, 16, 19]);
  test([10, 13, 16, 19],  7, 16, [ 7, 19]);
  test([10, 13, 16, 19],  8, 14, [ 8, 14, 16, 19]);
  test([10, 13, 16, 19],  8, 15, [ 8, 15, 16, 19]);
  test([10, 13, 16, 19],  8, 16, [ 8, 19]);
  test([10, 13, 16, 19],  9, 14, [ 9, 14, 16, 19]);
  test([10, 13, 16, 19],  9, 15, [ 9, 15, 16, 19]);
  test([10, 13, 16, 19],  9, 16, [ 9, 19]);
  test([10, 13, 16, 19], 10, 14, [10, 14, 16, 19]);
  test([10, 13, 16, 19], 10, 15, [10, 15, 16, 19]);
  test([10, 13, 16, 19], 10, 16, [10, 19]);
  test([10, 13, 16, 19], 11, 14, [10, 14, 16, 19]);
  test([10, 13, 16, 19], 11, 15, [10, 15, 16, 19]);
  test([10, 13, 16, 19], 11, 16, [10, 19]);
  test([10, 13, 16, 19], 12, 14, [10, 14, 16, 19]);
  test([10, 13, 16, 19], 12, 15, [10, 15, 16, 19]);
  test([10, 13, 16, 19], 12, 16, [10, 19]);
  test([10, 13, 16, 19], 13, 14, [10, 14, 16, 19]);
  test([10, 13, 16, 19], 13, 15, [10, 15, 16, 19]);
  test([10, 13, 16, 19], 13, 16, [10, 19]);
  test([10, 13, 16, 19], 14, 15, [10, 13, 14, 15, 16, 19]);
  test([10, 13, 16, 19], 14, 16, [10, 13, 14, 19]);
  test([10, 13, 16, 19], 15, 16, [10, 13, 15, 19]);

  run_next_test();
});

add_test(function test_ril_consts_cellbroadcast_misc() {
  // Must be 16 for indexing.
  equal(CB_DCS_LANG_GROUP_1.length, 16);
  equal(CB_DCS_LANG_GROUP_2.length, 16);

  // Array length must be even.
  equal(CB_NON_MMI_SETTABLE_RANGES.length & 0x01, 0);
  for (let i = 0; i < CB_NON_MMI_SETTABLE_RANGES.length;) {
    let from = CB_NON_MMI_SETTABLE_RANGES[i++];
    let to = CB_NON_MMI_SETTABLE_RANGES[i++];
    equal(from < to, true);
  }

  run_next_test();
});

add_test(function test_ril_worker_checkCellBroadcastMMISettable() {
  let worker = newWorker({
    postRILMessage: function(data) {
      // Do nothing
    },
    postMessage: function(message) {
      // Do nothing
    }
  });

  let context = worker.ContextPool._contexts[0];
  let ril = context.RIL;

  function test(from, to, expected) {
    equal(expected, ril._checkCellBroadcastMMISettable(from, to));
  }

  test(-2, -1, false);
  test(-1, 0, false);
  test(0, 1, true);
  test(1, 1, false);
  test(2, 1, false);
  test(65536, 65537, false);

  // We have both [4096, 4224), [4224, 4352), so it's actually [4096, 4352),
  // and [61440, 65536), [65535, 65536), so it's actually [61440, 65536).
  for (let i = 0; i < CB_NON_MMI_SETTABLE_RANGES.length;) {
    let from = CB_NON_MMI_SETTABLE_RANGES[i++];
    let to = CB_NON_MMI_SETTABLE_RANGES[i++];
    if ((from != 4224) && (from != 65535)) {
      test(from - 1, from, true);
    }
    test(from - 1, from + 1, false);
    test(from - 1, to, false);
    test(from - 1, to + 1, false);
    test(from, from + 1, false);
    test(from, to, false);
    test(from, to + 1, false);
    if ((from + 1) < to) {
      test(from + 1, to, false);
      test(from + 1, to + 1, false);
    }
    if ((to != 4224) && (to < 65535)) {
      test(to, to + 1, true);
      test(to + 1, to + 2, true);
    }
  }

  run_next_test();
});
