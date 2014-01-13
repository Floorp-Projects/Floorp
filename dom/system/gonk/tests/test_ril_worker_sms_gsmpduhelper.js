/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

function run_test() {
  run_next_test();
}

/**
 * Verify GsmPDUHelper#readDataCodingScheme.
 */
add_test(function test_GsmPDUHelper_readDataCodingScheme() {
  let worker = newWorker({
    postRILMessage: function(data) {
      // Do nothing
    },
    postMessage: function(message) {
      // Do nothing
    }
  });

  let helper = worker.GsmPDUHelper;
  function test_dcs(dcs, encoding, messageClass, mwi) {
    helper.readHexOctet = function() {
      return dcs;
    }

    let msg = {};
    helper.readDataCodingScheme(msg);

    do_check_eq(msg.dcs, dcs);
    do_check_eq(msg.encoding, encoding);
    do_check_eq(msg.messageClass, messageClass);
    do_check_eq(msg.mwi == null, mwi == null);
    if (mwi != null) {
      do_check_eq(msg.mwi.active, mwi.active);
      do_check_eq(msg.mwi.discard, mwi.discard);
      do_check_eq(msg.mwi.msgCount, mwi.msgCount);
    }
  }

  // Group 00xx
  //   Bit 3 and 2 indicate the character set being used.
  test_dcs(0x00, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  test_dcs(0x04, PDU_DCS_MSG_CODING_8BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  test_dcs(0x08, PDU_DCS_MSG_CODING_16BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  test_dcs(0x0C, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  //   Bit 4, if set to 0, indicates that bits 1 to 0 are reserved and have no
  //   message class meaning.
  test_dcs(0x01, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  test_dcs(0x02, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  test_dcs(0x03, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  //   Bit 4, if set to 1, indicates that bits 1 to 0 have a message class meaning.
  test_dcs(0x10, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_0]);
  test_dcs(0x11, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_1]);
  test_dcs(0x12, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_2]);
  test_dcs(0x13, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_3]);

  // Group 01xx
  test_dcs(0x50, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_0]);

  // Group 1000..1011: reserved
  test_dcs(0x8F, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  test_dcs(0x9F, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  test_dcs(0xAF, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);
  test_dcs(0xBF, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL]);

  // Group 1100: Message Waiting Indication Group: Discard Message
  //   Bit 3 indicates Indication Sense:
  test_dcs(0xC0, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
           {active: false, discard: true, msgCount: 0});
  test_dcs(0xC8, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
           {active: true, discard: true, msgCount: -1});
  //   Bit 2 is reserved, and set to 0:
  test_dcs(0xCC, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
           {active: true, discard: true, msgCount: -1});

  // Group 1101: Message Waiting Indication Group: Store Message
  //   Bit 3 indicates Indication Sense:
  test_dcs(0xD0, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
           {active: false, discard: false, msgCount: 0});
  test_dcs(0xD8, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
           {active: true, discard: false, msgCount: -1});
  //   Bit 2 is reserved, and set to 0:
  test_dcs(0xDC, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
           {active: true, discard: false, msgCount: -1});

  // Group 1110: Message Waiting Indication Group: Store Message, UCS2
  //   Bit 3 indicates Indication Sense:
  test_dcs(0xE0, PDU_DCS_MSG_CODING_16BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
           {active: false, discard: false, msgCount: 0});
  test_dcs(0xE8, PDU_DCS_MSG_CODING_16BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
           {active: true, discard: false, msgCount: -1});
  //   Bit 2 is reserved, and set to 0:
  test_dcs(0xEC, PDU_DCS_MSG_CODING_16BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL],
           {active: true, discard: false, msgCount: -1});

  // Group 1111
  test_dcs(0xF0, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_0]);
  test_dcs(0xF1, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_1]);
  test_dcs(0xF2, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_2]);
  test_dcs(0xF3, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_3]);
  test_dcs(0xF4, PDU_DCS_MSG_CODING_8BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_0]);
  test_dcs(0xF5, PDU_DCS_MSG_CODING_8BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_1]);
  test_dcs(0xF6, PDU_DCS_MSG_CODING_8BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_2]);
  test_dcs(0xF7, PDU_DCS_MSG_CODING_8BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_3]);
  //   Bit 3 is reserved and should be set to 0, but if it doesn't we should
  //   ignore it.
  test_dcs(0xF8, PDU_DCS_MSG_CODING_7BITS_ALPHABET,
           GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_0]);

  run_next_test();
});

/**
 * Verify GsmPDUHelper#writeStringAsSeptets() padding bits handling.
 */
add_test(function test_GsmPDUHelper_writeStringAsSeptets() {
  let worker = newWorker({
    postRILMessage: function(data) {
      // Do nothing
    },
    postMessage: function(message) {
      // Do nothing
    }
  });

  let helper = worker.GsmPDUHelper;
  helper.resetOctetWritten = function() {
    helper.octetsWritten = 0;
  };
  helper.writeHexOctet = function() {
    helper.octetsWritten++;
  };

  let base = "AAAAAAAA"; // Base string of 8 characters long
  for (let len = 0; len < 8; len++) {
    let str = base.substring(0, len);

    for (let paddingBits = 0; paddingBits < 8; paddingBits++) {
      do_print("Verifying GsmPDUHelper.writeStringAsSeptets("
               + str + ", " + paddingBits + ", <default>, <default>)");
      helper.resetOctetWritten();
      helper.writeStringAsSeptets(str, paddingBits, PDU_NL_IDENTIFIER_DEFAULT,
                                  PDU_NL_IDENTIFIER_DEFAULT);
      do_check_eq(Math.ceil(((len * 7) + paddingBits) / 8),
                  helper.octetsWritten);
    }
  }

  run_next_test();
});

/**
 * Verify GsmPDUHelper#readAddress
 */
add_test(function test_GsmPDUHelper_readAddress() {
  let worker = newWorker({
    postRILMessage: function(data) {
      // Do nothing
    },
    postMessage: function(message) {
      // Do nothing
    }

  });

  let helper = worker.GsmPDUHelper;
  function test_address(addrHex, addrString) {
    let uint16Array = [];
    let ix = 0;
    for (let i = 0; i < addrHex.length; ++i) {
      uint16Array[i] = addrHex[i].charCodeAt();
    }

    worker.Buf.readUint16 = function(){
      if(ix >= uint16Array.length) {
        do_throw("out of range in uint16Array");
      }
      return uint16Array[ix++];
    }
    let length = helper.readHexOctet();
    let parsedAddr = helper.readAddress(length);
    do_check_eq(parsedAddr, addrString);
  }

  // For AlphaNumeric
  test_address("04D01100", "_@");
  test_address("04D01000", "\u0394@");

  // Direct prepand
  test_address("0B914151245584F6", "+14154255486");
  test_address("0E914151245584B633", "+14154255486#33");

  // PDU_TOA_NATIONAL
  test_address("0BA14151245584F6", "14154255486");

  run_next_test();
});
