/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

subscriptLoader.loadSubScript("resource://gre/modules/ril_consts.js", this);

const ESCAPE = "\uffff";
const RESCTL = "\ufffe";

function run_test() {
  run_next_test();
}

/**
 * Verify receiving SMS-DELIVERY messages
 */

function hexToNibble(nibble) {
  nibble &= 0x0f;
  if (nibble < 10) {
    nibble += 48; // ASCII '0'
  } else {
    nibble += 55; // ASCII 'A'
  }
  return nibble;
}

function pduToParcelData(pdu) {
  let dataLength = 4 + pdu.length * 4 + 4;
  let data = new Uint8Array(dataLength);
  let offset = 0;

  // String length
  data[offset++] = pdu.length & 0xFF;
  data[offset++] = (pdu.length >> 8) & 0xFF;
  data[offset++] = (pdu.length >> 16) & 0xFF;
  data[offset++] = (pdu.length >> 24) & 0xFF;

  // PDU data
  for (let i = 0; i < pdu.length; i++) {
    let hi = (pdu[i] >>> 4) & 0x0F;
    let lo = pdu[i] & 0x0F;

    data[offset++] = hexToNibble(hi);
    data[offset++] = 0;
    data[offset++] = hexToNibble(lo);
    data[offset++] = 0;
  }

  // String delimitor
  data[offset++] = 0;
  data[offset++] = 0;
  data[offset++] = 0;
  data[offset++] = 0;

  return data;
}

function compose7bitPdu(lst, sst, data, septets) {
  if ((lst == 0) && (sst == 0)) {
    return [0x00,                              // SMSC
            PDU_MTI_SMS_DELIVER,               // firstOctet
            1, 0x00, 0,                        // senderAddress
            0x00,                              // protocolIdentifier
            PDU_DCS_MSG_CODING_7BITS_ALPHABET, // dataCodingScheme
            0, 0, 0, 0, 0, 0, 0,               // y m d h m s tz
            septets]                           // userDataLength
           .concat(data);
  }

  return [0x00,                                            // SMSC
          PDU_MTI_SMS_DELIVER | PDU_UDHI,                  // firstOctet
          1, 0x00, 0,                                      // senderAddress
          0x00,                                            // protocolIdentifier
          PDU_DCS_MSG_CODING_7BITS_ALPHABET,               // dataCodingScheme
          0, 0, 0, 0, 0, 0, 0,                             // y m d h m s tz
          8 + septets,                                     // userDataLength
          6,                                               // user data header length
          PDU_IEI_NATIONAL_LANGUAGE_LOCKING_SHIFT, 1, lst, // PDU_IEI_NATIONAL_LANGUAGE_LOCKING_SHIFT
          PDU_IEI_NATIONAL_LANGUAGE_SINGLE_SHIFT, 1, sst]  // PDU_IEI_NATIONAL_LANGUAGE_SINGLE_SHIFT
         .concat(data);
}

function composeUcs2Pdu(rawBytes) {
  return [0x00,                               // SMSC
          PDU_MTI_SMS_DELIVER,                // firstOctet
          1, 0x00, 0,                         // senderAddress
          0x00,                               // protocolIdentifier
          PDU_DCS_MSG_CODING_16BITS_ALPHABET, // dataCodingScheme
          0, 0, 0, 0, 0, 0, 0,                // y m d h m s tz
          rawBytes.length]                    // userDataLength
         .concat(rawBytes);
}

function newSmsParcel(pdu) {
  return newIncomingParcel(-1,
                           RESPONSE_TYPE_UNSOLICITED,
                           UNSOLICITED_RESPONSE_NEW_SMS,
                           pduToParcelData(pdu));
}

function removeSpecialChar(str, needle) {
  for (let i = 0; i < needle.length; i++) {
    let pos;
    while ((pos = str.indexOf(needle[i])) >= 0) {
      str = str.substring(0, pos) + str.substring(pos + 1);
    }
  }
  return str;
}

function newWriteHexOctetAsUint8Worker() {
  let worker = newWorker({
    postRILMessage: function(data) {
      // Do nothing
    },
    postMessage: function(message) {
      // Do nothing
    }
  });

  let context = worker.ContextPool._contexts[0];
  context.GsmPDUHelper.writeHexOctet = function(value) {
    context.Buf.writeUint8(value);
  };

  return worker;
}

function add_test_receiving_sms(expected, pdu) {
  add_test(function test_receiving_sms() {
    let worker = newWorker({
      postRILMessage: function(data) {
        // Do nothing
      },
      postMessage: function(message) {
        do_print("fullBody: " + message.fullBody);
        do_check_eq(expected, message.fullBody)
      }
    });

    do_print("expect: " + expected);
    do_print("pdu: " + pdu);
    worker.onRILMessage(0, newSmsParcel(pdu));

    run_next_test();
  });
}

let test_receiving_7bit_alphabets__ril;
let test_receiving_7bit_alphabets__worker;
function test_receiving_7bit_alphabets(lst, sst) {
  if (!test_receiving_7bit_alphabets__ril) {
    test_receiving_7bit_alphabets__ril = newRadioInterface();
    test_receiving_7bit_alphabets__worker = newWriteHexOctetAsUint8Worker();
  }
  let ril = test_receiving_7bit_alphabets__ril;
  let worker = test_receiving_7bit_alphabets__worker;
  let context = worker.ContextPool._contexts[0];
  let helper = context.GsmPDUHelper;
  let buf = context.Buf;

  function get7bitRawBytes(expected) {
    buf.outgoingIndex = 0;
    helper.writeStringAsSeptets(expected, 0, lst, sst);

    let subArray = buf.outgoingBytes.subarray(0, buf.outgoingIndex);
    return Array.slice(subArray);
  }

  let langTable = PDU_NL_LOCKING_SHIFT_TABLES[lst];
  let langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[sst];

  let text = removeSpecialChar(langTable + langShiftTable, ESCAPE + RESCTL);
  for (let i = 0; i < text.length;) {
    let len = Math.min(70, text.length - i);
    let expected = text.substring(i, i + len);
    let septets = ril._countGsm7BitSeptets(expected, langTable, langShiftTable);
    let rawBytes = get7bitRawBytes(expected);
    let pdu = compose7bitPdu(lst, sst, rawBytes, septets);
    add_test_receiving_sms(expected, pdu);

    i += len;
  }
}

function test_receiving_ucs2_alphabets(text) {
  let worker = test_receiving_7bit_alphabets__worker;
  let context = worker.ContextPool._contexts[0];
  let buf = context.Buf;

  function getUCS2RawBytes(expected) {
    buf.outgoingIndex = 0;
    context.GsmPDUHelper.writeUCS2String(expected);

    let subArray = buf.outgoingBytes.subarray(0, buf.outgoingIndex);
    return Array.slice(subArray);
  }

  for (let i = 0; i < text.length;) {
    let len = Math.min(70, text.length - i);
    let expected = text.substring(i, i + len);
    let rawBytes = getUCS2RawBytes(expected);
    let pdu = composeUcs2Pdu(rawBytes);
    add_test_receiving_sms(expected, pdu);

    i += len;
  }
}

let ucs2str = "";
for (let lst = 0; lst < PDU_NL_LOCKING_SHIFT_TABLES.length; lst++) {
  ucs2str += PDU_NL_LOCKING_SHIFT_TABLES[lst];
  for (let sst = 0; sst < PDU_NL_SINGLE_SHIFT_TABLES.length; sst++) {
    test_receiving_7bit_alphabets(lst, sst);

    if (lst == 0) {
      ucs2str += PDU_NL_SINGLE_SHIFT_TABLES[sst];
    }
  }
}
test_receiving_ucs2_alphabets(ucs2str);

// Bug 820220: B2G SMS: wrong order and truncated content in multi-part messages
add_test(function test_sendSMS_UCS2_without_langIndex_langShiftIndex_defined() {
  let worker = newWriteHexOctetAsUint8Worker();
  let context = worker.ContextPool._contexts[0];

  context.Buf.sendParcel = function() {
    // Each sendParcel() call represents one outgoing segment of a multipart
    // SMS message. Here, we have the first segment send, so it's "Hello "
    // only.
    //
    // 4(parcel size) + 4(request type) + 4(token)
    // + 4(two messages) + 4(null SMSC) + 4(message string length)
    // + 1(first octet) + 1(message reference)
    // + 2(DA len, TOA) + 4(addr)
    // + 1(pid) + 1(dcs)
    // + 1(UDL) + 6(UDHL, type, len, ref, max, seq)
    // + 12(2 * strlen("Hello "))
    // + 4(two delimitors) = 57
    //
    // If we have additional 6(type, len, langIndex, type len, langShiftIndex)
    // octets here, then bug 809553 is not fixed.
    do_check_eq(this.outgoingIndex, 57);

    run_next_test();
  };

  context.RIL.sendSMS({
    number: "1",
    segmentMaxSeq: 2,
    fullBody: "Hello World!",
    dcs: PDU_DCS_MSG_CODING_16BITS_ALPHABET,
    segmentRef16Bit: false,
    userDataHeaderLength: 5,
    requestStatusReport: true,
    segments: [
      {
        body: "Hello ",
        encodedBodyLength: 12,
      }, {
        body: "World!",
        encodedBodyLength: 12,
      }
    ],
  });
});
