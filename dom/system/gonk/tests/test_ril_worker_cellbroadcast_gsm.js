/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

add_test(function test_ril_worker_GsmPDUHelper_readCbDataCodingScheme() {
  let worker = newWorker({
    postRILMessage: function(data) {
      // Do nothing
    },
    postMessage: function(message) {
      // Do nothing
    }
  });

  let context = worker.ContextPool._contexts[0];
  function test_dcs(dcs, encoding, language, hasLanguageIndicator, messageClass) {
    context.Buf.readUint8 = function() {
      return dcs;
    };

    let msg = {};
    context.GsmPDUHelper.readCbDataCodingScheme(msg);

    equal(msg.dcs, dcs);
    equal(msg.encoding, encoding);
    equal(msg.language, language);
    equal(msg.hasLanguageIndicator, hasLanguageIndicator);
    equal(msg.messageClass, messageClass);
  }

  function test_dcs_throws(dcs) {
    context.Buf.readUint8 = function() {
      return dcs;
    };

    throws(function() {
      context.GsmPDUHelper.readCbDataCodingScheme({});
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
    postRILMessage: function(data) {
      // Do nothing
    },
    postMessage: function(message) {
      // Do nothing
    }
  });

  let context = worker.ContextPool._contexts[0];
  function test_data(options, expected) {
    let readIndex = 0;
    context.Buf.readUint8 = function() {
      return options[3][readIndex++];
    };
    context.Buf.readUint8Array = function(length) {
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
    context.GsmPDUHelper.readGsmCbData(msg, options[3].length);

    equal(msg.body, expected[0]);
    equal(msg.data == null, expected[1] == null);
    if (expected[1] != null) {
      equal(msg.data.length, expected[1].length);
      for (let i = 0; i < expected[1].length; i++) {
        equal(msg.data[i], expected[1][i]);
      }
    }
    equal(msg.language, expected[2]);
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
