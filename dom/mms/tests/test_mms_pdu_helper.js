/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let MMS = {};
subscriptLoader.loadSubScript("resource://gre/modules/MmsPduHelper.jsm", MMS);
MMS.debug = do_print;

function run_test() {
  run_next_test();
}

//
// Test target: BooleanValue
//

//// BooleanValue.decode ////

add_test(function test_BooleanValue_decode() {
  for (let i = 0; i < 256; i++) {
    if (i == 128) {
      wsp_decode_test(MMS.BooleanValue, [128], true);
    } else if (i == 129) {
      wsp_decode_test(MMS.BooleanValue, [129], false);
    } else {
      wsp_decode_test(MMS.BooleanValue, [i], null, "CodeError");
    }
  }

  run_next_test();
});

//// BooleanValue.encode ////

add_test(function test_BooleanValue_encode() {
  wsp_encode_test(MMS.BooleanValue, true, [128]);
  wsp_encode_test(MMS.BooleanValue, false, [129]);

  run_next_test();
});

//
// Test target: Address
//

//// Address.decode ////

add_test(function test_Address_decode() {
  // Test for PLMN address
  wsp_decode_test(MMS.Address, strToCharCodeArray("+123.456-789/TYPE=PLMN"),
                      {address: "+123.456-789", type: "PLMN"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("123456789/TYPE=PLMN"),
                      {address: "123456789", type: "PLMN"});
  // Test for IPv4
  wsp_decode_test(MMS.Address, strToCharCodeArray("1.2.3.4/TYPE=IPv4"),
                      {address: "1.2.3.4", type: "IPv4"});
  // Test for IPv6
  wsp_decode_test(MMS.Address,
    strToCharCodeArray("1111:AAAA:bbbb:CdEf:1ABC:2cde:3Def:0000/TYPE=IPv6"),
    {address: "1111:AAAA:bbbb:CdEf:1ABC:2cde:3Def:0000", type: "IPv6"}
  );
  // Test for other device-address
  wsp_decode_test(MMS.Address, strToCharCodeArray("+H-e.l%l_o/TYPE=W0r1d_"),
                      {address: "+H-e.l%l_o", type: "W0r1d_"});
  // Test for num-shortcode
  wsp_decode_test(MMS.Address, strToCharCodeArray("+123"),
                      {address: "+123", type: "num"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("*123"),
                      {address: "*123", type: "num"});
  wsp_decode_test(MMS.Address, strToCharCodeArray("#123"),
                      {address: "#123", type: "num"});
  // Test for alphanum-shortcode
  wsp_decode_test(MMS.Address, strToCharCodeArray("H0wD0Y0uTurnTh1s0n"),
                      {address: "H0wD0Y0uTurnTh1s0n", type: "alphanum"});
  // Test for email address
  wsp_decode_test(MMS.Address, strToCharCodeArray("Joe User <joe@user.org>"),
                      {address: "Joe User <joe@user.org>", type: "email"});
  // Test for invalid address
  wsp_decode_test(MMS.Address, strToCharCodeArray("@@@@@"),
                  null, "CodeError");

  run_next_test();
});

//// Address.encode ////

add_test(function test_Address_encode() {
  // Test for PLMN address
  wsp_encode_test(MMS.Address, {address: "+123.456-789", type: "PLMN"},
                  strToCharCodeArray("+123.456-789/TYPE=PLMN"));
  wsp_encode_test(MMS.Address, {address: "123456789", type: "PLMN"},
                  strToCharCodeArray("123456789/TYPE=PLMN"));
  // Test for IPv4
  wsp_encode_test(MMS.Address, {address: "1.2.3.4", type: "IPv4"},
                  strToCharCodeArray("1.2.3.4/TYPE=IPv4"));
  // Test for IPv6
  wsp_encode_test(MMS.Address,
    {address: "1111:AAAA:bbbb:CdEf:1ABC:2cde:3Def:0000", type: "IPv6"},
    strToCharCodeArray("1111:AAAA:bbbb:CdEf:1ABC:2cde:3Def:0000/TYPE=IPv6")
  );
  // Test for other device-address
  wsp_encode_test(MMS.Address, {address: "+H-e.l%l_o", type: "W0r1d_"},
                  strToCharCodeArray("+H-e.l%l_o/TYPE=W0r1d_"));
  // Test for num-shortcode
  wsp_encode_test(MMS.Address, {address: "+123", type: "num"},
                  strToCharCodeArray("+123"));
  wsp_encode_test(MMS.Address, {address: "*123", type: "num"},
                  strToCharCodeArray("*123"));
  wsp_encode_test(MMS.Address, {address: "#123", type: "num"},
                  strToCharCodeArray("#123"));
  // Test for alphanum-shortcode
  wsp_encode_test(MMS.Address, {address: "H0wD0Y0uTurnTh1s0n", type: "alphanum"},
                  strToCharCodeArray("H0wD0Y0uTurnTh1s0n"));
  // Test for email address
  wsp_encode_test(MMS.Address, {address: "Joe User <joe@user.org>", type: "email"},
                  strToCharCodeArray("Joe User <joe@user.org>"));

  run_next_test();
});

//
// Test target: HeaderField
//

//// HeaderField.decode ////

add_test(function test_HeaderField_decode() {
  wsp_decode_test(MMS.HeaderField, [65, 0, 66, 0], {name: "a", value: "B"});
  wsp_decode_test(MMS.HeaderField, [0x80 | 0x27, 128],
                      {name: "x-mms-stored", value: true});

  run_next_test();
});

//// HeaderField.encode ////

add_test(function test_HeaderField_encode() {
  // Test for MmsHeader
  wsp_encode_test(MMS.HeaderField, {name: "X-Mms-Message-Type",
                                    value: MMS_PDU_TYPE_SEND_REQ},
                  [0x80 | 0x0C, MMS_PDU_TYPE_SEND_REQ]);
  // Test for ApplicationHeader
  wsp_encode_test(MMS.HeaderField, {name: "a", value: "B"}, [97, 0, 66, 0]);

  run_next_test();
});

//
// Test target: MmsHeader
//

//// MmsHeader.decode ////

add_test(function test_MmsHeader_decode() {
  wsp_decode_test(MMS.MmsHeader, [0x80 | 0x00], null, "NotWellKnownEncodingError");
  wsp_decode_test(MMS.MmsHeader, [0x80 | 0x27, 128],
                      {name: "x-mms-stored", value: true});
  wsp_decode_test(MMS.MmsHeader, [0x80 | 0x27, 255], null);

  run_next_test();
});

//// MmsHeader.encode ////

add_test(function test_MmsHeader_encode() {
  // Test for empty header name:
  wsp_encode_test(MMS.MmsHeader, {name: undefined, value: null}, null, "CodeError");
  wsp_encode_test(MMS.MmsHeader, {name: null, value: null}, null, "CodeError");
  wsp_encode_test(MMS.MmsHeader, {name: "", value: null}, null, "CodeError");
  // Test for non-well-known header name:
  wsp_encode_test(MMS.MmsHeader, {name: "X-No-Such-Field", value: null},
                  null, "NotWellKnownEncodingError");
  // Test for normal header
  wsp_encode_test(MMS.MmsHeader, {name: "X-Mms-Message-Type",
                                  value: MMS_PDU_TYPE_SEND_REQ},
                  [0x80 | 0x0C, MMS_PDU_TYPE_SEND_REQ]);

  run_next_test();
});

//
// Test target: ContentClassValue
//

//// ContentClassValue.decode ////

add_test(function test_ContentClassValue_decode() {
  for (let i = 0; i < 256; i++) {
    if ((i >= 128) && (i <= 135)) {
      wsp_decode_test(MMS.ContentClassValue, [i], i);
    } else {
      wsp_decode_test(MMS.ContentClassValue, [i], null, "CodeError");
    }
  }

  run_next_test();
});

//// ContentClassValue.encode ////

add_test(function test_ContentClassValue_encode() {
  for (let i = 0; i < 256; i++) {
    if ((i >= 128) && (i <= 135)) {
      wsp_encode_test(MMS.ContentClassValue, i, [i]);
    } else {
      wsp_encode_test(MMS.ContentClassValue, i, null, "CodeError");
    }
  }

  run_next_test();
});

//
// Test target: ContentLocationValue
//

//// ContentLocationValue.decode ////

add_test(function test_ContentLocationValue_decode() {
  // Test for MMS_PDU_TYPE_MBOX_DELETE_CONF & MMS_PDU_TYPE_DELETE_CONF
  function test(type, statusCount, exception) {
    function decode(data) {
      let options = {};
      if (type) {
        options["x-mms-message-type"] = type;
      }
      return MMS.ContentLocationValue.decode(data, options);
    }

    let uri = "http://no.such.com/path";

    let data = strToCharCodeArray(uri);
    if (statusCount != null) {
      data = [data.length + 1, statusCount | 0x80].concat(data);
    }

    let expected;
    if (!exception) {
      expected = {};
      if (statusCount != null) {
        expected.statusCount = statusCount;
      }
      expected.uri = uri;
    }

    do_print("data = " + JSON.stringify(data));
    wsp_decode_test_ex(decode, data, expected, exception);
  }

  test(null, null, "FatalCodeError");
  for (let type = MMS_PDU_TYPE_SEND_REQ; type <= MMS_PDU_TYPE_CANCEL_CONF; type++) {
    if ((type == MMS_PDU_TYPE_MBOX_DELETE_CONF)
        || (type == MMS_PDU_TYPE_DELETE_CONF)) {
      test(type, 1, null);
    } else {
      test(type, null, null);
    }
  }

  run_next_test();
});

//
// Test target: ElementDescriptorValue
//

//// ElementDescriptorValue.decode ////

add_test(function test_ElementDescriptorValue_decode() {
  wsp_decode_test(MMS.ElementDescriptorValue, [2, 97, 0], {contentReference: "a"});
  wsp_decode_test(MMS.ElementDescriptorValue, [4, 97, 0, 0x80 | 0x02, 0x80],
                      {contentReference: "a", params: {type: 0}});

  run_next_test();
});

//
// Test target: Parameter
//

//// Parameter.decodeParameterName ////

add_test(function test_Parameter_decodeParameterName() {
  wsp_decode_test_ex(function (data) {
      return MMS.Parameter.decodeParameterName(data);
    }, [0x80 | 0x02], "type"
  );
  wsp_decode_test_ex(function (data) {
      return MMS.Parameter.decodeParameterName(data);
    }, strToCharCodeArray("type"), "type"
  );

  run_next_test();
});

//// Parameter.decode ////

add_test(function test_Parameter_decode() {
  wsp_decode_test(MMS.Parameter, [0x80 | 0x02, 0x80 | 0x00], {name: "type", value: 0});

  run_next_test();
});

//// Parameter.decodeMultiple ////

add_test(function test_Parameter_decodeMultiple() {
  // FIXME: The following test case falls because Parameter-value decoding of
  //        "type" parameters utilies WSP.ConstrainedEncoding, which in turn
  //        utilies WSP.TextString, and TextString is not matual exclusive to
  //        each other.
  //wsp_decode_test_ex(function (data) {
  //    return MMS.Parameter.decodeMultiple(data, data.array.length);
  //  }, [0x80 | 0x02, 0x80 | 0x00].concat(strToCharCodeArray("good")).concat([0x80 | 0x01]),
  //  {type: 0, good: 1}
  //);

  run_next_test();
});

//// Parameter.encode ////

add_test(function test_Parameter_encode() {
  // Test for invalid parameter value
  wsp_encode_test(MMS.Parameter, null, null, "CodeError");
  wsp_encode_test(MMS.Parameter, undefined, null, "CodeError");
  wsp_encode_test(MMS.Parameter, {}, null, "CodeError");
  // Test for case-insensitive parameter name
  wsp_encode_test(MMS.Parameter, {name: "TYPE", value: 0}, [130, 128]);
  wsp_encode_test(MMS.Parameter, {name: "type", value: 0}, [130, 128]);
  // Test for non-well-known parameter name
  wsp_encode_test(MMS.Parameter, {name: "name", value: 0}, [110, 97, 109, 101, 0, 128]);
  // Test for constrained encoding value
  wsp_encode_test(MMS.Parameter, {name: "type", value: "0"}, [130, 48, 0]);

  run_next_test();
});

//
// Test target: EncodedStringValue
//

//// EncodedStringValue.decode ////

add_test(function test_EncodedStringValue_decode() {
  // Test for normal TextString
  wsp_decode_test(MMS.EncodedStringValue, strToCharCodeArray("Hello"), "Hello");
  // Test for non-well-known charset
  wsp_decode_test(MMS.EncodedStringValue, [1, 0x80], null, "NotWellKnownEncodingError");
  // Test for utf-8
  let (entry = MMS.WSP.WSP_WELL_KNOWN_CHARSETS["utf-8"]) {
    // "Mozilla" in full width.
    let str = "\uff2d\uff4f\uff5a\uff49\uff4c\uff4c\uff41";

    let conv = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
               .createInstance(Ci.nsIScriptableUnicodeConverter);
    conv.charset = entry.converter;

    let raw = conv.convertToByteArray(str).concat([0]);
    wsp_decode_test(MMS.EncodedStringValue,
                    [raw.length + 2, 0x80 | entry.number, 127].concat(raw), str);
  }

  run_next_test();
});

//// EncodedStringValue.encode ////

add_test(function test_EncodedStringValue_encode() {
  // Test for normal TextString
  wsp_encode_test(MMS.EncodedStringValue, "Hello", strToCharCodeArray("Hello"));
  // Test for utf-8
  let (entry = MMS.WSP.WSP_WELL_KNOWN_CHARSETS["utf-8"]) {
    // "Mozilla" in full width.
    let str = "\uff2d\uff4f\uff5a\uff49\uff4c\uff4c\uff41";

    let conv = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
               .createInstance(Ci.nsIScriptableUnicodeConverter);
    conv.charset = entry.converter;

    let raw = conv.convertToByteArray(str).concat([0]);
    wsp_encode_test(MMS.EncodedStringValue, str,
                    [raw.length + 2, 0x80 | entry.number, 127].concat(raw));
  }

  run_next_test();
});

//
// Test target: ExpiryValue
//

//// ExpiryValue.decode ////

add_test(function test_ExpiryValue_decode() {
  // Test for Absolute-token Date-value
  wsp_decode_test(MMS.ExpiryValue, [3, 128, 1, 0x80], new Date(0x80 * 1000));
  // Test for Relative-token Delta-seconds-value
  wsp_decode_test(MMS.ExpiryValue, [2, 129, 0x80], 0);

  run_next_test();
});

//// ExpiryValue.encode ////

add_test(function test_ExpiryValue_encode() {
  // Test for Absolute-token Date-value
  wsp_encode_test(MMS.ExpiryValue, new Date(0x80 * 1000), [3, 128, 1, 0x80]);
  // Test for Relative-token Delta-seconds-value
  wsp_encode_test(MMS.ExpiryValue, 0, [2, 129, 0x80]);

  run_next_test();
});

//
// Test target: PreviouslySentByValue
//

//// PreviouslySentByValue.decode ////

add_test(function test_PreviouslySentByValue_decode() {
  wsp_decode_test(MMS.PreviouslySentByValue, [3, 0x80 | 0x03, 65, 0],
                      {forwardedCount: 3, originator: {address: "A",
                                                       type: "alphanum"}});

  run_next_test();
});

//
// Test target: PreviouslySentDateValue
//

//// PreviouslySentDateValue.decode ////

add_test(function test_PreviouslySentDateValue_decode() {
  wsp_decode_test(MMS.PreviouslySentDateValue, [3, 0x80 | 0x03, 1, 4],
                      {forwardedCount: 3, timestamp: new Date(4 * 1000)});

  run_next_test();
});

//
// Test target: FromValue
//

//// FromValue.decode ////

add_test(function test_FromValue_decode() {
  // Test for Insert-address-token:
  wsp_decode_test(MMS.FromValue, [1, 129], null);
  // Test for Address-present-token:
  let (addr = strToCharCodeArray("+123/TYPE=PLMN")) {
    wsp_decode_test(MMS.FromValue, [addr.length + 1, 128].concat(addr),
                        {address: "+123", type: "PLMN"});
  }

  run_next_test();
});

//// FromValue.encode ////

add_test(function test_FromValue_encode() {
  // Test for Insert-address-token:
  wsp_encode_test(MMS.FromValue, null, [1, 129]);
  // Test for Address-present-token:
  let (addr = strToCharCodeArray("+123/TYPE=PLMN")) {
    wsp_encode_test(MMS.FromValue, {address: "+123", type: "PLMN"},
                    [addr.length + 1, 128].concat(addr));
  }

  run_next_test();
});

//
// Test target: MessageClassValue
//

//// MessageClassValue.decodeClassIdentifier ////

add_test(function test_MessageClassValue_decodeClassIdentifier() {
  let (IDs = ["personal", "advertisement", "informational", "auto"]) {
    for (let i = 0; i < 256; i++) {
      if ((i >= 128) && (i <= 131)) {
        wsp_decode_test_ex(function (data) {
            return MMS.MessageClassValue.decodeClassIdentifier(data);
          }, [i], IDs[i - 128]
        );
      } else {
        wsp_decode_test_ex(function (data) {
            return MMS.MessageClassValue.decodeClassIdentifier(data);
          }, [i], null, "CodeError"
        );
      }
    }
  }

  run_next_test();
});

//// MessageClassValue.decode ////

add_test(function test_MessageClassValue_decode() {
  wsp_decode_test(MMS.MessageClassValue, [65, 0], "A");
  wsp_decode_test(MMS.MessageClassValue, [128], "personal");

  run_next_test();
});

//// MessageClassValue.encode ////

add_test(function test_MessageClassValue_encode() {
  wsp_encode_test(MMS.MessageClassValue, "personal", [128]);
  wsp_encode_test(MMS.MessageClassValue, "advertisement", [129]);
  wsp_encode_test(MMS.MessageClassValue, "informational", [130]);
  wsp_encode_test(MMS.MessageClassValue, "auto", [131]);
  wsp_encode_test(MMS.MessageClassValue, "A", [65, 0]);

  run_next_test();
});

//
// Test target: MessageTypeValue
//

//// MessageTypeValue.decode ////

add_test(function test_MessageTypeValue_decode() {
  for (let i = 0; i < 256; i++) {
    if ((i >= 128) && (i <= 151)) {
      wsp_decode_test(MMS.MessageTypeValue, [i], i);
    } else {
      wsp_decode_test(MMS.MessageTypeValue, [i], null, "CodeError");
    }
  }

  run_next_test();
});

//// MessageTypeValue.encode ////

add_test(function test_MessageTypeValue_encode() {
  for (let i = 0; i < 256; i++) {
    if ((i >= 128) && (i <= 151)) {
      wsp_encode_test(MMS.MessageTypeValue, i, [i]);
    } else {
      wsp_encode_test(MMS.MessageTypeValue, i, null, "CodeError");
    }
  }

  run_next_test();
});

//
// Test target: MmFlagsValue
//

//// MmFlagsValue.decode ////

add_test(function test_MmFlagsValue_decode() {
  for (let i = 0; i < 256; i++) {
    if ((i >= 128) && (i <= 130)) {
      wsp_decode_test(MMS.MmFlagsValue, [3, i, 65, 0], {type: i, text: "A"});
    } else {
      wsp_decode_test(MMS.MmFlagsValue, [3, i, 65, 0], null, "CodeError");
    }
  }

  run_next_test();
});

//// MmFlagsValue.encode ////

add_test(function test_MmFlagsValue_encode() {
  for (let i = 0; i < 256; i++) {
    if ((i >= 128) && (i <= 130)) {
      wsp_encode_test(MMS.MmFlagsValue, {type: i, text: "A"}, [3, i, 65, 0]);
    } else {
      wsp_encode_test(MMS.MmFlagsValue, {type: i, text: "A"}, null, "CodeError");
    }
  }

  run_next_test();
});

//
// Test target: MmStateValue
//

//// MmStateValue.decode ////

add_test(function test_MmStateValue_decode() {
  for (let i = 0; i < 256; i++) {
    if ((i >= 128) && (i <= 132)) {
      wsp_decode_test(MMS.MmStateValue, [i], i);
    } else {
      wsp_decode_test(MMS.MmStateValue, [i], null, "CodeError");
    }
  }

  run_next_test();
});

//// MmStateValue.encode ////

add_test(function test_MmStateValue_encode() {
  for (let i = 0; i < 256; i++) {
    if ((i >= 128) && (i <= 132)) {
      wsp_encode_test(MMS.MmStateValue, i, [i]);
    } else {
      wsp_encode_test(MMS.MmStateValue, i, null, "CodeError");
    }
  }

  run_next_test();
});

//
// Test target: PriorityValue
//

//// PriorityValue.decode ////

add_test(function test_PriorityValue_decode() {
  for (let i = 0; i < 256; i++) {
    if ((i >= 128) && (i <= 130)) {
      wsp_decode_test(MMS.PriorityValue, [i], i);
    } else {
      wsp_decode_test(MMS.PriorityValue, [i], null, "CodeError");
    }
  }

  run_next_test();
});

//// PriorityValue.encode ////

add_test(function test_PriorityValue_encode() {
  for (let i = 0; i < 256; i++) {
    if ((i >= 128) && (i <= 130)) {
      wsp_encode_test(MMS.PriorityValue, i, [i]);
    } else {
      wsp_encode_test(MMS.PriorityValue, i, null, "CodeError");
    }
  }

  run_next_test();
});

//
// Test target: RecommendedRetrievalModeValue
//

//// RecommendedRetrievalModeValue.decode ////

add_test(function test_RecommendedRetrievalModeValue_decode() {
  for (let i = 0; i < 256; i++) {
    if (i == 128) {
      wsp_decode_test(MMS.RecommendedRetrievalModeValue, [i], i);
    } else {
      wsp_decode_test(MMS.RecommendedRetrievalModeValue, [i], null, "CodeError");
    }
  }

  run_next_test();
});

//
// Test target: ReplyChargingValue
//

//// ReplyChargingValue.decode ////

add_test(function test_ReplyChargingValue_decode() {
  for (let i = 0; i < 256; i++) {
    if ((i >= 128) && (i <= 131)) {
      wsp_decode_test(MMS.ReplyChargingValue, [i], i);
    } else {
      wsp_decode_test(MMS.ReplyChargingValue, [i], null, "CodeError");
    }
  }

  run_next_test();
});

//// ReplyChargingValue.encode ////

add_test(function test_ReplyChargingValue_encode() {
  for (let i = 0; i < 256; i++) {
    if ((i >= 128) && (i <= 131)) {
      wsp_encode_test(MMS.ReplyChargingValue, i, [i]);
    } else {
      wsp_encode_test(MMS.ReplyChargingValue, i, null, "CodeError");
    }
  }

  run_next_test();
});

//
// Test target: ResponseText
//

//// ResponseText.decode ////

add_test(function test_ResponseText_decode() {
  // Test for MMS_PDU_TYPE_MBOX_DELETE_CONF & MMS_PDU_TYPE_DELETE_CONF
  wsp_decode_test_ex(function (data) {
      data.array[0] = data.array.length - 1;

      let options = {};
      options["x-mms-message-type"] = MMS_PDU_TYPE_MBOX_DELETE_CONF;
      return MMS.ResponseText.decode(data, options);
    }, [0, 0x80 | 0x00].concat(strToCharCodeArray("http://no.such.com/path")),
    {statusCount: 0, text: "http://no.such.com/path"}
  );
  wsp_decode_test_ex(function (data) {
      data.array[0] = data.array.length - 1;

      let options = {};
      options["x-mms-message-type"] = MMS_PDU_TYPE_DELETE_CONF;
      return MMS.ResponseText.decode(data, options);
    }, [0, 0x80 | 0x00].concat(strToCharCodeArray("http://no.such.com/path")),
    {statusCount: 0, text: "http://no.such.com/path"}
  );
  // Test for other situations
  wsp_decode_test_ex(function (data) {
      let options = {};
      options["x-mms-message-type"] = MMS_PDU_TYPE_SEND_REQ;
      return MMS.ResponseText.decode(data, options);
    }, strToCharCodeArray("http://no.such.com/path"),
    {text: "http://no.such.com/path"}
  );

  run_next_test();
});

//
// Test target: RetrieveStatusValue
//

//// RetrieveStatusValue.decode ////

add_test(function test_RetrieveStatusValue_decode() {
  for (let i = 0; i < 256; i++) {
    if ((i == MMS_PDU_ERROR_OK)
        || (i >= MMS_PDU_ERROR_TRANSIENT_FAILURE)) {
      wsp_decode_test(MMS.RetrieveStatusValue, [i], i);
    } else {
      wsp_decode_test(MMS.RetrieveStatusValue, [i],
                      MMS_PDU_ERROR_PERMANENT_FAILURE);
    }
  }

  run_next_test();
});

//
// Test target: StatusValue
//

//// StatusValue.decode ////

add_test(function test_StatusValue_decode() {
  for (let i = 0; i < 256; i++) {
    if ((i >= 128) && (i <= 135)) {
      wsp_decode_test(MMS.StatusValue, [i], i);
    } else {
      wsp_decode_test(MMS.StatusValue, [i], null, "CodeError");
    }
  }

  run_next_test();
});

//// StatusValue.encode ////

add_test(function test_StatusValue_encode() {
  for (let i = 0; i < 256; i++) {
    if ((i >= 128) && (i <= 135)) {
      wsp_encode_test(MMS.StatusValue, i, [i]);
    } else {
      wsp_encode_test(MMS.StatusValue, i, null, "CodeError");
    }
  }

  run_next_test();
});

//
// Test target: PduHelper
//

//// PduHelper.parseHeaders ////

add_test(function test_PduHelper_parseHeaders() {
  function parse(input, expect, exception) {
    let data = {array: input, offset: 0};
    do_check_throws(wsp_test_func.bind(null, MMS.PduHelper.parseHeaders, data, expect),
                    exception);
  }

  // Parse ends with Content-Type
  let expect = {};
  expect["x-mms-mms-version"] = MMS_VERSION;
  expect["content-type"] = {
    media: "application/vnd.wap.multipart.related",
    params: null,
  };
  parse([0x80 | 0x0D, 0x80 | MMS_VERSION,   // X-Mms-Mms-Version: 1.3
         0x80 | 0x04, 0x80 | 0x33,          // Content-Type: application/vnd.wap.multipart.related
         0x80 | 0x0C, MMS_PDU_TYPE_SEND_REQ // X-Mms-Message-Type: M-Send.req
        ], expect);

  // Parse header fields with multiple entries
  expect = {
    to: [
      { address: "+123", type: "PLMN" },
      { address: "+456", type: "num" },
    ],
  };
  expect["content-type"] = {
    media: "application/vnd.wap.multipart.related",
    params: null,
  };
  parse(Array.concat([0x80 | 0x17]).concat(strToCharCodeArray("+123/TYPE=PLMN"))
             .concat([0x80 | 0x17]).concat(strToCharCodeArray("+456"))
             .concat([0x80 | 0x04, 0x80 | 0x33]),
        expect);

  run_next_test();
});

//// PduHelper.encodeHeader ////

add_test(function test_PduHelper_encodeHeader() {
  function func(name, data, headers) {
    MMS.PduHelper.encodeHeader(data, headers, name);

    // Remove extra space consumed during encoding.
    while (data.array.length > data.offset) {
      data.array.pop();
    }

    return data.array;
  }

  // Encode header fields with multiple entries
  let headers = {
    to: [
      { address: "+123", type: "PLMN" },
      { address: "+456", type: "num" },
    ],
  };
  wsp_encode_test_ex(func.bind(null, "to"), headers,
                     Array.concat([0x80 | 0x17]).concat(strToCharCodeArray("+123/TYPE=PLMN"))
                          .concat([0x80 | 0x17]).concat(strToCharCodeArray("+456")));

  run_next_test();
});

//// PduHelper.encodeHeaderIfExists ////

add_test(function test_PduHelper_encodeHeaderIfExists() {
  function func(name, data, headers) {
    MMS.PduHelper.encodeHeaderIfExists(data, headers, name);

    // Remove extra space consumed during encoding.
    while (data.array.length > data.offset) {
      data.array.pop();
    }

    return data.array;
  }

  wsp_encode_test_ex(func.bind(null, "to"), {}, []);

  run_next_test();
});

//// PduHelper.encodeHeaders ////

add_test(function test_PduHelper_encodeHeaders() {
  function func(data, headers) {
    MMS.PduHelper.encodeHeaders(data, headers);

    // Remove extra space consumed during encoding.
    while (data.array.length > data.offset) {
      data.array.pop();
    }

    return data.array;
  }

  let headers = {};
  headers["x-mms-message-type"] = MMS_PDU_TYPE_SEND_REQ;
  headers["x-mms-mms-version"] = MMS_VERSION;
  headers["x-mms-transaction-id"] = "asdf";
  headers["to"] = { address: "+123", type: "PLMN" };
  headers["content-type"] = {
    media: "application/vnd.wap.multipart.related",
  };
  wsp_encode_test_ex(func, headers,
                     Array.concat([0x80 | 0x0C, MMS_PDU_TYPE_SEND_REQ])
                          .concat([0x80 | 0x18]).concat(strToCharCodeArray(headers["x-mms-transaction-id"]))
                          .concat([0x80 | 0x0D, 0x80 | MMS_VERSION])
                          .concat([0x80 | 0x17]).concat(strToCharCodeArray("+123/TYPE=PLMN"))
                          .concat([0x80 | 0x04, 0x80 | 0x33]));

  run_next_test();
});

