/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var WSP = {};
subscriptLoader.loadSubScript("resource://gre/modules/WspPduHelper.jsm", WSP);
WSP.debug = do_print;

function run_test() {
  run_next_test();
}

//
// Test target: ensureHeader
//

add_test(function test_ensureHeader() {
  do_check_throws(function() {
      WSP.ensureHeader({}, "no-such-property");
    }, "FatalCodeError"
  );

  run_next_test();
});

//
// Test target: Header
//

//// Header.decode ////

add_test(function test_Header_decode() {
  wsp_decode_test(WSP.Header, [0x34 | 0x80, 0x80], {name: "push-flag", value: 0});
  wsp_decode_test(WSP.Header, [65, 0, 66, 0], {name: "a", value: "B"});

  run_next_test();
});

//
// Test target: WellKnownHeader
//

//// WellKnownHeader.decode ////

add_test(function test_WellKnownHeader_decode() {
  wsp_decode_test(WSP.WellKnownHeader, [0xFF], null, "NotWellKnownEncodingError");
  let entry = WSP.WSP_HEADER_FIELDS["push-flag"];

  // Test for Short-Integer
  wsp_decode_test(WSP.WellKnownHeader, [entry.number | 0x80, 0x80],
                      {name: entry.name, value: 0});
  // Test for NoValue
  wsp_decode_test(WSP.WellKnownHeader, [entry.number | 0x80, 0],
                      {name: entry.name, value: null});
  // Test for TokenText
  wsp_decode_test(WSP.WellKnownHeader, [entry.number | 0x80, 65, 0],
                      {name: entry.name, value: "A"});
  // Test for QuotedString
  wsp_decode_test(WSP.WellKnownHeader, [entry.number | 0x80, 34, 128, 0],
                      {name: entry.name, value: String.fromCharCode(128)});
  // Test for skipValue
  wsp_decode_test(WSP.WellKnownHeader, [entry.number | 0x80, 2, 0, 0], null);

  run_next_test();
});

//
// Test target: ApplicationHeader
//

//// ApplicationHeader.decode ////

add_test(function test_ApplicationHeader_decode() {
  wsp_decode_test(WSP.ApplicationHeader, [5, 0, 66, 0], null, "CodeError");
  wsp_decode_test(WSP.ApplicationHeader, [65, 0, 66, 0], {name: "a", value: "B"});
  // Test for skipValue
  wsp_decode_test(WSP.ApplicationHeader, [65, 0, 2, 0, 0], null);

  run_next_test();
});

//// ApplicationHeader.encode ////

add_test(function test_ApplicationHeader_encode() {
  // Test invalid header name string:
  wsp_encode_test(WSP.ApplicationHeader, {name: undefined, value: "asdf"}, null, "CodeError");
  wsp_encode_test(WSP.ApplicationHeader, {name: null, value: "asdf"}, null, "CodeError");
  wsp_encode_test(WSP.ApplicationHeader, {name: "", value: "asdf"}, null, "CodeError");
  wsp_encode_test(WSP.ApplicationHeader, {name: "a b", value: "asdf"}, null, "CodeError");
  // Test value string:
  wsp_encode_test(WSP.ApplicationHeader, {name: "asdf", value: undefined},
                  strToCharCodeArray("asdf").concat([0]));
  wsp_encode_test(WSP.ApplicationHeader, {name: "asdf", value: null},
                  strToCharCodeArray("asdf").concat([0]));
  wsp_encode_test(WSP.ApplicationHeader, {name: "asdf", value: ""},
                  strToCharCodeArray("asdf").concat([0]));
  wsp_encode_test(WSP.ApplicationHeader, {name: "asdf", value: "fdsa"},
                  strToCharCodeArray("asdf").concat(strToCharCodeArray("fdsa")));

  run_next_test();
});

//
// Test target: FieldName
//

//// FieldName.decode ////

add_test(function test_FieldName_decode() {
  wsp_decode_test(WSP.FieldName, [0], "");
  wsp_decode_test(WSP.FieldName, [65, 0], "a");
  wsp_decode_test(WSP.FieldName, [97, 0], "a");
  let entry = WSP.WSP_HEADER_FIELDS["content-length"];
  wsp_decode_test(WSP.FieldName, [entry.number | 0x80], entry.name);
  wsp_decode_test(WSP.FieldName, [0xFF], null, "NotWellKnownEncodingError");

  run_next_test();
});

//// FieldName.encode ////

add_test(function test_FieldName_encode() {
  wsp_encode_test(WSP.FieldName, "", [0]);
  wsp_encode_test(WSP.FieldName, "date", [0x92]);

  run_next_test();
});

//
// Test target: PduHelper
//

//// PduHelper.parseHeaders ////

add_test(function test_PduHelper_parseHeaders() {
  wsp_decode_test_ex(function(data) {
      return WSP.PduHelper.parseHeaders(data, data.array.length);
    }, [0x80 | 0x05, 2, 0x23, 0x28, 0x80 | 0x2F, 0x80 | 0x04],
    {"age": 9000, "x-wap-application-id": "x-wap-application:mms.ua"}
  );

  run_next_test();
});

//// PduHelper.decodeStringContent ////

add_test(function StringContent_decode() {
  //Test for utf-8
  let entry = WSP.WSP_WELL_KNOWN_CHARSETS["utf-8"];

  // "Mozilla" in full width.
  let str = "\uff2d\uff4f\uff5a\uff49\uff4c\uff4c\uff41";

  let conv = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
             .createInstance(Ci.nsIScriptableUnicodeConverter);
  conv.charset = entry.converter;

  let raw = conv.convertToByteArray(str);
  let data = {array: raw, offset: 0};
  let octetArray = WSP.Octet.decodeMultiple(data, data.array.length);
  wsp_decode_test_ex(function(data) {
      return WSP.PduHelper.decodeStringContent(data.array, "utf-8");
    }, octetArray, str);

  entry = WSP.WSP_WELL_KNOWN_CHARSETS["utf-16"];
  // "Mozilla" in full width.
  str = "\u004d\u006F\u007A\u0069\u006C\u006C\u0061";

  conv = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
         .createInstance(Ci.nsIScriptableUnicodeConverter);
  conv.charset = entry.converter;

  raw = conv.convertToByteArray(str);
  data = {array: raw, offset: 0};
  octetArray = WSP.Octet.decodeMultiple(data, data.array.length);
  wsp_decode_test_ex(function(data) {
      return WSP.PduHelper.decodeStringContent(data.array, "utf-16");
    }, raw, str);

  run_next_test();
});

//// PduHelper.composeMultiPart ////

add_test(function test_PduHelper_composeMultiPart() {
  let multiStream = Components.classes["@mozilla.org/io/multiplex-input-stream;1"]
                    .createInstance(Ci.nsIMultiplexInputStream);
  let uint8Array = new Uint8Array(5);
  uint8Array[0] = 0x00;
  uint8Array[1] = 0x01;
  uint8Array[2] = 0x02;
  uint8Array[3] = 0x03;
  uint8Array[4] = 0x04;

  let parts = [
      {
        content: "content",
        headers: {
            "content-type": {
                media: "text/plain",
                params: {}
            }
        }
      },
      {
        content: uint8Array,
        headers: {
            "content-type": {
                media: "text/plain",
                params: {}
            }
        }
      }
    ];

  let beforeCompose = JSON.stringify(parts);
  WSP.PduHelper.composeMultiPart(multiStream, parts);
  let afterCompose = JSON.stringify(parts);

  do_check_eq(beforeCompose, afterCompose);
  run_next_test();
});
