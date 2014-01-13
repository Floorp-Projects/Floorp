/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

add_test(function test_ril_consts_cellbroadcast_misc() {
  // Must be 16 for indexing.
  do_check_eq(CB_DCS_LANG_GROUP_1.length, 16);
  do_check_eq(CB_DCS_LANG_GROUP_2.length, 16);

  // Array length must be even.
  do_check_eq(CB_NON_MMI_SETTABLE_RANGES.length & 0x01, 0);
  for (let i = 0; i < CB_NON_MMI_SETTABLE_RANGES.length;) {
    let from = CB_NON_MMI_SETTABLE_RANGES[i++];
    let to = CB_NON_MMI_SETTABLE_RANGES[i++];
    do_check_eq(from < to, true);
  }

  run_next_test();
});

add_test(function test_ril_worker_GsmPDUHelper_readCbDataCodingScheme() {
  let worker = newWorker({
    postRILMessage: function fakePostRILMessage(data) {
      // Do nothing
    },
    postMessage: function fakePostMessage(message) {
      // Do nothing
    }
  });

  function test_dcs(dcs, encoding, language, hasLanguageIndicator, messageClass) {
    worker.Buf.readUint8 = function() {
      return dcs;
    };

    let msg = {};
    worker.GsmPDUHelper.readCbDataCodingScheme(msg);

    do_check_eq(msg.dcs, dcs);
    do_check_eq(msg.encoding, encoding);
    do_check_eq(msg.language, language);
    do_check_eq(msg.hasLanguageIndicator, hasLanguageIndicator);
    do_check_eq(msg.messageClass, messageClass);
  }

  function test_dcs_throws(dcs) {
    worker.Buf.readUint8 = function() {
      return dcs;
    };

    do_check_throws(function() {
      worker.GsmPDUHelper.readCbDataCodingScheme({});
    }, "Unsupported CBS data coding scheme: " + dcs);
  }

  // Group 0000
  for (let i = 0; i < 16; i++) {
    test_dcs(i, PDU_DCS_MSG_CODING_7BITS_ALPHABET, CB_DCS_LANG_GROUP_1[i],
             false, GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  }

  // Group 0001
  //   0000 GSM 7 bit default alphabet; message preceded by language indication.
  test_dcs(0x10, PDU_DCS_MSG_CODING_7BITS_ALPHABET, null, true,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  //   0001 UCS2; message preceded by language indication.
  test_dcs(0x11, PDU_DCS_MSG_CODING_16BITS_ALPHABET, null, true,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);

  // Group 0010
  //   0000..0100
  for (let i = 0; i < 5; i++) {
    test_dcs(0x20 + i, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
             CB_DCS_LANG_GROUP_2[i], false,
             GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  }
  //   0101..1111 Reserved
  for (let i = 5; i < 16; i++) {
    test_dcs(0x20 + i, PDU_DCS_MSG_CODING_7BITS_ALPHABET, null, false,
             GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  }

  // Group 0100, 0101, 1001
  for (let group of [0x40, 0x50, 0x90]) {
    for (let i = 0; i < 16; i++) {
      let encoding = i & 0x0C;
      if (encoding == 0x0C) {
        encoding = PDU_DCS_MSG_CODING_7BITS_ALPHABET;
      }
      let messageClass = GECKO_SMS_MESSAGE_CLASSES[i & PDU_DCS_MSG_CLASS_BITS];
      test_dcs(group + i, encoding, null, false, messageClass);
    }
  }

  // Group 1111
  for (let i = 0; i < 16; i ++) {
    let encoding = i & 0x04 ? PDU_DCS_MSG_CODING_8BITS_ALPHABET
                            : PDU_DCS_MSG_CODING_7BITS_ALPHABET;
    let messageClass;
    switch(i & PDU_DCS_MSG_CLASS_BITS) {
      case 0x01: messageClass = PDU_DCS_MSG_CLASS_USER_1; break;
      case 0x02: messageClass = PDU_DCS_MSG_CLASS_USER_2; break;
      case 0x03: messageClass = PDU_DCS_MSG_CLASS_3; break;
      default: messageClass = PDU_DCS_MSG_CLASS_NORMAL; break;
    }
    test_dcs(0xF0 + i, encoding, null, false,
             GECKO_SMS_MESSAGE_CLASSES[messageClass]);
  }

  // Group 0011, 1000, 1010, 1011, 1100
  //   0000..1111 Reserved
  for (let group of [0x30, 0x80, 0xA0, 0xB0, 0xC0]) {
    for (let i = 0; i < 16; i++) {
      test_dcs(group + i, PDU_DCS_MSG_CODING_7BITS_ALPHABET, null, false,
               GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
    }
  }

  // Group 0110, 0111, 1101, 1110
  //   TODO: unsupported
  for (let group of [0x60, 0x70, 0xD0, 0xE0]) {
    for (let i = 0; i < 16; i++) {
      test_dcs_throws(group + i);
    }
  }

  run_next_test();
});

add_test(function test_ril_worker_GsmPDUHelper_readGsmCbData() {
  let worker = newWorker({
    postRILMessage: function fakePostRILMessage(data) {
      // Do nothing
    },
    postMessage: function fakePostMessage(message) {
      // Do nothing
    }
  });

  function test_data(options, expected) {
    let readIndex = 0;
    worker.Buf.readUint8 = function() {
      return options[3][readIndex++];
    };
    worker.Buf.readUint8Array = function(length) {
      let array = new Uint8Array(length);
      for (let i = 0; i < length; i++) {
        array[i] = this.readUint8();
      }
      return array;
    };

    let msg = {
      encoding: options[0],
      language: options[1],
      hasLanguageIndicator: options[2]
    };
    worker.GsmPDUHelper.readGsmCbData(msg, options[3].length);

    do_check_eq(msg.body, expected[0]);
    do_check_eq(msg.data == null, expected[1] == null);
    if (expected[1] != null) {
      do_check_eq(msg.data.length, expected[1].length);
      for (let i = 0; i < expected[1].length; i++) {
        do_check_eq(msg.data[i], expected[1][i]);
      }
    }
    do_check_eq(msg.language, expected[2]);
  }

  // We're testing Cell Broadcast message body with all zeros octet stream. As
  // shown in 3GPP TS 23.038, septet 0x00 will be decoded as '@' when both
  // langTableIndex and langShiftTableIndex equal to
  // PDU_DCS_MSG_CODING_7BITS_ALPHABET.

  // PDU_DCS_MSG_CODING_7BITS_ALPHABET
  test_data([PDU_DCS_MSG_CODING_7BITS_ALPHABET, null, false,
              [0]],
            ["@", null, null]);
  test_data([PDU_DCS_MSG_CODING_7BITS_ALPHABET, null, true,
              [0, 0, 0, 0]],
            ["@", null, "@@"]);
  test_data([PDU_DCS_MSG_CODING_7BITS_ALPHABET, "@@", false,
              [0]],
            ["@", null, "@@"]);

  // PDU_DCS_MSG_CODING_8BITS_ALPHABET
  test_data([PDU_DCS_MSG_CODING_8BITS_ALPHABET, null, false,
              [0]],
            [null, [0], null]);

  // PDU_DCS_MSG_CODING_16BITS_ALPHABET
  test_data([PDU_DCS_MSG_CODING_16BITS_ALPHABET, null, false,
              [0x00, 0x40]],
            ["@", null, null]);
  test_data([PDU_DCS_MSG_CODING_16BITS_ALPHABET, null, true,
              [0x00, 0x00, 0x00, 0x40]],
            ["@", null, "@@"]);
  test_data([PDU_DCS_MSG_CODING_16BITS_ALPHABET, "@@", false,
              [0x00, 0x40]],
            ["@", null, "@@"]);

  run_next_test();
});

add_test(function test_ril_worker__checkCellBroadcastMMISettable() {
  let worker = newWorker({
    postRILMessage: function fakePostRILMessage(data) {
      // Do nothing
    },
    postMessage: function fakePostMessage(message) {
      // Do nothing
    }
  });

  let ril = worker.RIL;

  function test(from, to, expected) {
    do_check_eq(expected, ril._checkCellBroadcastMMISettable(from, to));
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

add_test(function test_ril_worker__mergeCellBroadcastConfigs() {
  let worker = newWorker({
    postRILMessage: function fakePostRILMessage(data) {
      // Do nothing
    },
    postMessage: function fakePostMessage(message) {
      // Do nothing
    }
  });

  let ril = worker.RIL;

  function test(olist, from, to, expected) {
    let result = ril._mergeCellBroadcastConfigs(olist, from, to);
    do_check_eq(JSON.stringify(expected), JSON.stringify(result));
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

