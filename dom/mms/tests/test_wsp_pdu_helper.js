/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let WSP = {};
subscriptLoader.loadSubScript("resource://gre/modules/WspPduHelper.jsm", WSP);
WSP.debug = do_print;

function run_test() {
  run_next_test();
}

//
// Test target: ensureHeader
//

add_test(function test_ensureHeader() {
  do_check_throws(function () {
      WSP.ensureHeader({}, "no-such-property");
    }, "FatalCodeError"
  );

  run_next_test();
});

//
// Test target: skipValue()
//

add_test(function test_skipValue() {
  // Test for zero-valued first octet:
  wsp_decode_test_ex(function (data) {
      return WSP.skipValue(data);
    }, [0], null
  );
  // Test first octet < 31
  wsp_decode_test_ex(function (data) {
      return WSP.skipValue(data);
    }, [1, 2], [2]
  );
  // Test first octet = 31
  wsp_decode_test_ex(function (data) {
      return WSP.skipValue(data);
    }, [31, 0], null
  );
  wsp_decode_test_ex(function (data) {
      return WSP.skipValue(data);
    }, [31, 1, 2], [2]
  );
  // Test first octet <= 127
  wsp_decode_test_ex(function (data) {
      return WSP.skipValue(data);
    }, strToCharCodeArray("Hello world!"), "Hello world!"
  );
  // Test first octet >= 128
  wsp_decode_test_ex(function (data) {
      return WSP.skipValue(data);
    }, [0x80 | 0x01], 0x01
  );

  run_next_test();
});

//
// Test target: Octet
//

//// Octet.decode ////

add_test(function test_Octet_decode() {
  wsp_decode_test(WSP.Octet, [1], 1);
  wsp_decode_test(WSP.Octet, [], null, "RangeError");

  run_next_test();
});

//// Octet.decodeMultiple ////

add_test(function test_Octet_decodeMultiple() {
  wsp_decode_test_ex(function (data) {
      return WSP.Octet.decodeMultiple(data, 3);
    }, [0, 1, 2], [0, 1, 2], null
  );
  wsp_decode_test_ex(function (data) {
      return WSP.Octet.decodeMultiple(data, 3);
    }, new Uint8Array([0, 1, 2]), new Uint8Array([0, 1, 2]), null
  );
  wsp_decode_test_ex(function (data) {
      return WSP.Octet.decodeMultiple(data, 4);
    }, [0, 1, 2], null, "RangeError"
  );

  run_next_test();
});

//// Octet.decodeEqualTo ////

add_test(function test_Octet_decodeEqualTo() {
  wsp_decode_test_ex(function (data) {
      return WSP.Octet.decodeEqualTo(data, 1);
    }, [1], 1, null
  );
  wsp_decode_test_ex(function (data) {
      return WSP.Octet.decodeEqualTo(data, 2);
    }, [1], null, "CodeError"
  );
  wsp_decode_test_ex(function (data) {
      return WSP.Octet.decodeEqualTo(data, 2);
    }, [], null, "RangeError"
  );

  run_next_test();
});

//// Octet.encode ////

add_test(function test_Octet_encode() {
  for (let i = 0; i < 256; i++) {
    wsp_encode_test(WSP.Octet, i, [i]);
  }

  run_next_test();
});

//
// Test target: Text
//

//// Text.decode ////

add_test(function test_Text_decode() {
  for (let i = 0; i < 256; i++) {
    if (i == 0) {
      wsp_decode_test(WSP.Text, [0], null, "NullCharError");
    } else if ((i < WSP.CTLS) || (i == WSP.DEL)) {
      wsp_decode_test(WSP.Text, [i], null, "CodeError");
    } else {
      wsp_decode_test(WSP.Text, [i], String.fromCharCode(i));
    }
  }
  // Test \r\n(SP|HT)* sequence:
  wsp_decode_test(WSP.Text, strToCharCodeArray("\r\n \t \t \t", true), " ");
  wsp_decode_test(WSP.Text, strToCharCodeArray("\r\n \t \t \t"), " ");
  wsp_decode_test(WSP.Text, strToCharCodeArray("\r\n \t \t \tA"), " ");

  run_next_test();
});

//// Text.encode ////

add_test(function test_Text_encode() {
  for (let i = 0; i < 256; i++) {
    if ((i < WSP.CTLS) || (i == WSP.DEL)) {
      wsp_encode_test(WSP.Text, String.fromCharCode(i), null, "CodeError");
    } else {
      wsp_encode_test(WSP.Text, String.fromCharCode(i), [i]);
    }
  }

  run_next_test();
});

//
// Test target: NullTerminatedTexts
//

//// NullTerminatedTexts.decode ////

add_test(function test_NullTerminatedTexts_decode() {
  // Test incompleted string:
  wsp_decode_test(WSP.NullTerminatedTexts, strToCharCodeArray(" ", true), null, "RangeError");
  // Test control char:
  wsp_decode_test(WSP.NullTerminatedTexts, strToCharCodeArray(" \n"), null, "CodeError");
  // Test normal string:
  wsp_decode_test(WSP.NullTerminatedTexts, strToCharCodeArray(""), "");
  wsp_decode_test(WSP.NullTerminatedTexts, strToCharCodeArray("oops"), "oops");
  // Test \r\n(SP|HT)* sequence:
  wsp_decode_test(WSP.NullTerminatedTexts, strToCharCodeArray("A\r\n \t \t \tB"), "A B");

  run_next_test();
});

//// NullTerminatedTexts.encode ////

add_test(function test_NullTerminatedTexts_encode() {
  wsp_encode_test(WSP.NullTerminatedTexts, "", [0]);
  wsp_encode_test(WSP.NullTerminatedTexts, "Hello, World!",
                  strToCharCodeArray("Hello, World!"));

  run_next_test();
});

//
// Test target: Token
//

let TOKEN_SEPS = "()<>@,;:\\\"/[]?={} \t";

//// Token.decode ////

add_test(function test_Token_decode() {
  for (let i = 0; i < 256; i++) {
    if (i == 0) {
      wsp_decode_test(WSP.Token, [i], null, "NullCharError");
    } else if ((i < WSP.CTLS) || (i >= WSP.ASCIIS)
        || (TOKEN_SEPS.indexOf(String.fromCharCode(i)) >= 0)) {
      wsp_decode_test(WSP.Token, [i], null, "CodeError");
    } else {
      wsp_decode_test(WSP.Token, [i], String.fromCharCode(i));
    }
  }

  run_next_test();
});

//// Token.encode ////

add_test(function test_Token_encode() {
  for (let i = 0; i < 256; i++) {
    if ((i < WSP.CTLS) || (i >= WSP.ASCIIS)
        || (TOKEN_SEPS.indexOf(String.fromCharCode(i)) >= 0)) {
      wsp_encode_test(WSP.Token, String.fromCharCode(i), null, "CodeError");
    } else {
      wsp_encode_test(WSP.Token, String.fromCharCode(i), [i]);
    }
  }

  run_next_test();
});

//
// Test target: URIC
//

//// URIC.decode ////

add_test(function test_URIC_decode() {
  let uric = "!#$%&'()*+,-./0123456789:;=?@ABCDEFGHIJKLMN"
             + "OPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz~";
  for (let i = 0; i < 256; i++) {
    if (i == 0) {
      wsp_decode_test(WSP.URIC, [i], null, "NullCharError");
    } else if (uric.indexOf(String.fromCharCode(i)) >= 0) {
      wsp_decode_test(WSP.URIC, [i], String.fromCharCode(i));
    } else {
      wsp_decode_test(WSP.URIC, [i], null, "CodeError");
    }
  }

  run_next_test();
});

//
// Test target: TextString
//

//// TextString.decode ////

add_test(function test_TextString_decode() {
  // Test quoted string
  wsp_decode_test(WSP.TextString, [127, 128, 0], String.fromCharCode(128));
  // Test illegal quoted string
  wsp_decode_test(WSP.TextString, [127, 32, 0], null, "CodeError");
  // Test illegal unquoted string
  wsp_decode_test(WSP.TextString, [128, 0], null, "CodeError");
  // Test normal string
  wsp_decode_test(WSP.TextString, [32, 0], " ");

  run_next_test();
});

//// TextString.encode ////

add_test(function test_TextString_encode() {
  // Test quoted string
  wsp_encode_test(WSP.TextString, String.fromCharCode(128), [127, 128, 0]);
  // Test normal string
  wsp_encode_test(WSP.TextString, "Mozilla", strToCharCodeArray("Mozilla"));

  run_next_test();
});

//
// Test target: TokenText
//

//// TokenText.decode ////

add_test(function test_TokenText_decode() {
  wsp_decode_test(WSP.TokenText, [65], null, "RangeError");
  wsp_decode_test(WSP.TokenText, [0], "");
  wsp_decode_test(WSP.TokenText, [65, 0], "A");

  run_next_test();
});

//// TokenText.encode ////

add_test(function test_TokenText_encode() {
  wsp_encode_test(WSP.TokenText, "B2G", strToCharCodeArray("B2G"));

  run_next_test();
});

//
// Test target: QuotedString
//

//// QuotedString.decode ////

add_test(function test_QuotedString_decode() {
  // Test non-quoted string
  wsp_decode_test(WSP.QuotedString, [32, 0], null, "CodeError");
  // Test incompleted string
  wsp_decode_test(WSP.QuotedString, [34, 32], null, "RangeError");
  wsp_decode_test(WSP.QuotedString, [34, 32, 0], " ");

  run_next_test();
});

//
// Test target: ShortInteger
//

//// ShortInteger.decode ////

add_test(function test_ShortInteger_decode() {
  for (let i = 0; i < 256; i++) {
    if (i & 0x80) {
      wsp_decode_test(WSP.ShortInteger, [i], i & 0x7F);
    } else {
      wsp_decode_test(WSP.ShortInteger, [i], null, "CodeError");
    }
  }

  run_next_test();
});

//// ShortInteger.encode ////

add_test(function test_ShortInteger_encode() {
  for (let i = 0; i < 256; i++) {
    if (i & 0x80) {
      wsp_encode_test(WSP.ShortInteger, i, null, "CodeError");
    } else {
      wsp_encode_test(WSP.ShortInteger, i, [0x80 | i]);
    }
  }

  run_next_test();
});

//
// Test target: LongInteger
//

//// LongInteger.decode ////

function LongInteger_testcases(target) {
  // Test LongInteger of zero octet
  wsp_decode_test(target, [0, 0], null, "CodeError");
  wsp_decode_test(target, [1, 0x80], 0x80);
  wsp_decode_test(target, [2, 0x80, 2], 0x8002);
  wsp_decode_test(target, [3, 0x80, 2, 3], 0x800203);
  wsp_decode_test(target, [4, 0x80, 2, 3, 4], 0x80020304);
  wsp_decode_test(target, [5, 0x80, 2, 3, 4, 5], 0x8002030405);
  wsp_decode_test(target, [6, 0x80, 2, 3, 4, 5, 6], 0x800203040506);
  // Test LongInteger of more than 6 octets
  wsp_decode_test(target, [7, 0x80, 2, 3, 4, 5, 6, 7], [0x80, 2, 3, 4, 5, 6, 7]);
  // Test LongInteger of more than 30 octets
  wsp_decode_test(target, [31], null, "CodeError");
}
add_test(function test_LongInteger_decode() {
  LongInteger_testcases(WSP.LongInteger);

  run_next_test();
});

//
// Test target: UintVar
//

//// UintVar.decode ////

add_test(function test_UintVar_decode() {
  wsp_decode_test(WSP.UintVar, [0x80], null, "RangeError");
  // Test up to max 53 bits integer
  wsp_decode_test(WSP.UintVar, [0x7F], 0x7F);
  wsp_decode_test(WSP.UintVar, [0xFF, 0x7F], 0x3FFF);
  wsp_decode_test(WSP.UintVar, [0xFF, 0xFF, 0x7F], 0x1FFFFF);
  wsp_decode_test(WSP.UintVar, [0xFF, 0xFF, 0xFF, 0x7F], 0xFFFFFFF);
  wsp_decode_test(WSP.UintVar, [0xFF, 0xFF, 0xFF, 0xFF, 0x7F], 0x7FFFFFFFF);
  wsp_decode_test(WSP.UintVar, [0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F], 0x3FFFFFFFFFF);
  wsp_decode_test(WSP.UintVar, [0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F], 0x1FFFFFFFFFFFF);
  wsp_decode_test(WSP.UintVar, [0x8F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F], 0x1FFFFFFFFFFFFF);
  wsp_decode_test(WSP.UintVar, [0x01, 0x02], 1);
  wsp_decode_test(WSP.UintVar, [0x80, 0x01, 0x02], 1);
  wsp_decode_test(WSP.UintVar, [0x80, 0x80, 0x80, 0x01, 0x2], 1);

  run_next_test();
});

//
// Test target: ConstrainedEncoding (decodeAlternatives)
//

//// ConstrainedEncoding.decode ////

add_test(function test_ConstrainedEncoding_decode() {
  wsp_decode_test(WSP.ConstrainedEncoding, [0x80], 0);
  wsp_decode_test(WSP.ConstrainedEncoding, [32, 0], " ");

  run_next_test();
});

//
// Test target: ValueLength
//

//// ValueLength.decode ////

add_test(function test_ValueLength_decode() {
  for (let i = 0; i < 256; i++) {
    if (i < 31) {
      wsp_decode_test(WSP.ValueLength, [i, 0x8F, 0x7F], i);
    } else if (i == 31) {
      wsp_decode_test(WSP.ValueLength, [i, 0x8F, 0x7F], 0x7FF);
    } else {
      wsp_decode_test(WSP.ValueLength, [i, 0x8F, 0x7F], null, "CodeError");
    }
  }

  run_next_test();
});

//
// Test target: NoValue
//

//// NoValue.decode ////

add_test(function test_NoValue_decode() {
  wsp_decode_test(WSP.NoValue, [0], null);
  for (let i = 1; i < 256; i++) {
    wsp_decode_test(WSP.NoValue, [i], null, "CodeError");
  }

  run_next_test();
});

//
// Test target: TextValue
//

//// TextValue.decode ////

add_test(function test_TextValue_decode() {
  wsp_decode_test(WSP.TextValue, [0], null);
  wsp_decode_test(WSP.TextValue, [65, 0], "A");
  wsp_decode_test(WSP.TextValue, [32, 0], null, "CodeError");
  wsp_decode_test(WSP.TextValue, [34, 32, 0], " ");

  run_next_test();
});

//
// Test target: IntegerValue
//

//// IntegerValue.decode ////

add_test(function test_IntegerValue_decode() {
  for (let i = 128; i < 256; i++) {
    wsp_decode_test(WSP.IntegerValue, [i], i & 0x7F);
  }

  LongInteger_testcases(WSP.IntegerValue);

  run_next_test();
});

//
// Test target: DateValue
//

//// DateValue.decode ////

add_test(function test_DateValue_decode() {
  wsp_decode_test(WSP.DateValue, [0, 0], null, "CodeError");
  wsp_decode_test(WSP.DateValue, [1, 0x80], new Date(0x80 * 1000));
  wsp_decode_test(WSP.DateValue, [31], null, "CodeError");

  run_next_test();
});

//
// Test target: DeltaSecondsValue
//
  // DeltaSecondsValue is only an alias of IntegerValue.

//
// Test target: QValue
//

//// QValue.decode ////

add_test(function test_QValue_decode() {
  wsp_decode_test(WSP.QValue, [0], null, "CodeError");
  wsp_decode_test(WSP.QValue, [1], 0);
  wsp_decode_test(WSP.QValue, [100], 0.99);
  wsp_decode_test(WSP.QValue, [101], 0.001);
  wsp_decode_test(WSP.QValue, [0x88, 0x4B], 0.999);
  wsp_decode_test(WSP.QValue, [0x88, 0x4C], null, "CodeError");

  run_next_test();
});

//
// Test target: VersionValue
//

//// VersionValue.decode ////

add_test(function test_VersionValue_decode() {
  for (let major = 1; major < 8; major++) {
    let version = (major << 4) | 0x0F;
    wsp_decode_test(WSP.VersionValue, [0x80 | version], version);
    wsp_decode_test(WSP.VersionValue, [major + 0x30, 0], version);

    for (let minor = 0; minor < 15; minor++) {
      version = (major << 4) | minor;
      wsp_decode_test(WSP.VersionValue, [0x80 | version], version);
      if (minor >= 10) {
        wsp_decode_test(WSP.VersionValue, [major + 0x30, 0x2E, 0x31, (minor - 10) + 0x30, 0], version);
      } else {
        wsp_decode_test(WSP.VersionValue, [major + 0x30, 0x2E, minor + 0x30, 0], version);
      }
    }
  }

  run_next_test();
});

//
// Test target: UriValue
//

//// UriValue.decode ////

add_test(function test_UriValue_decode() {
  wsp_decode_test(WSP.UriValue, [97], null, "RangeError");
  wsp_decode_test(WSP.UriValue, [0], "");
  wsp_decode_test(WSP.UriValue, [65, 0], "A");

  run_next_test();
});

//
// Test target: Parameter
//

//// Parameter.decodeTypedParameter ////

add_test(function test_Parameter_decodeTypedParameter() {
  // Test for array-typed return value from IntegerValue
  wsp_decode_test_ex(function (data) {
      return WSP.Parameter.decodeTypedParameter(data);
    }, [7, 0, 0, 0, 0, 0, 0, 0], null, "CodeError"
  );
  // Test for number-typed return value from IntegerValue
  wsp_decode_test_ex(function (data) {
      return WSP.Parameter.decodeTypedParameter(data);
    }, [1, 0, 0], {name: "q", value: null}
  );
  // Test for NotWellKnownEncodingError
  wsp_decode_test_ex(function (data) {
      return WSP.Parameter.decodeTypedParameter(data);
    }, [1, 0xFF], null, "NotWellKnownEncodingError"
  );
  // Test for parameter specific decoder
  wsp_decode_test_ex(function (data) {
      return WSP.Parameter.decodeTypedParameter(data);
    }, [1, 0, 100], {name: "q", value: 0.99}
  );
  // Test for TextValue
  wsp_decode_test_ex(function (data) {
      return WSP.Parameter.decodeTypedParameter(data);
    }, [1, 0x10, 48, 46, 57, 57, 0], {name: "secure", value: "0.99"}
  );
  // Test for TextString
  wsp_decode_test_ex(function (data) {
      return WSP.Parameter.decodeTypedParameter(data);
    }, [1, 0x19, 60, 115, 109, 105, 108, 62, 0], {name: "start", value: "<smil>"}
  );
  // Test for skipValue
  wsp_decode_test_ex(function (data) {
      return WSP.Parameter.decodeTypedParameter(data);
    }, [1, 0x19, 128], null
  );

  run_next_test();
});

//// Parameter.decodeUntypedParameter ////

add_test(function test_Parameter_decodeUntypedParameter() {
  wsp_decode_test_ex(function (data) {
      return WSP.Parameter.decodeUntypedParameter(data);
    }, [1], null, "CodeError"
  );
  wsp_decode_test_ex(function (data) {
      return WSP.Parameter.decodeUntypedParameter(data);
    }, [65, 0, 0], {name: "a", value: null}
  );
  // Test for IntegerValue
  wsp_decode_test_ex(function (data) {
      return WSP.Parameter.decodeUntypedParameter(data);
    }, [65, 0, 1, 0], {name: "a", value: 0}
  );
  // Test for TextValue
  wsp_decode_test_ex(function (data) {
      return WSP.Parameter.decodeUntypedParameter(data);
    }, [65, 0, 66, 0], {name: "a", value: "B"}
  );

  run_next_test();
});

//// Parameter.decode ////

add_test(function test_Parameter_decode() {
  wsp_decode_test_ex(function (data) {
      return WSP.Parameter.decode(data);
    }, [1, 0x19, 60, 115, 109, 105, 108, 62, 0], {name: "start", value: "<smil>"}
  );
  wsp_decode_test_ex(function (data) {
      return WSP.Parameter.decode(data);
    }, [65, 0, 66, 0], {name: "a", value: "B"}
  );

  run_next_test();
});

//// Parameter.decodeMultiple ////

add_test(function test_Parameter_decodeMultiple() {
  wsp_decode_test_ex(function (data) {
      return WSP.Parameter.decodeMultiple(data, 13);
    }, [1, 0x19, 60, 115, 109, 105, 108, 62, 0, 65, 0, 66, 0], {start: "<smil>", a: "B"}
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
  let (entry = WSP.WSP_HEADER_FIELDS["push-flag"]) {
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
  }

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
  let (entry = WSP.WSP_HEADER_FIELDS["content-length"]) {
    wsp_decode_test(WSP.FieldName, [entry.number | 0x80], entry.name);
  }
  wsp_decode_test(WSP.FieldName, [0xFF], null, "NotWellKnownEncodingError");

  run_next_test();
});

//
// Test target: AcceptCharsetValue
//

//// AcceptCharsetValue.decode ////

add_test(function test_AcceptCharsetValue_decode() {
  wsp_decode_test(WSP.AcceptCharsetValue, [0xFF], null, "CodeError");
  // Test for Any-Charset
  wsp_decode_test(WSP.AcceptCharsetValue, [128], {charset: "*"});
  // Test for Constrained-Charset
  wsp_decode_test(WSP.AcceptCharsetValue, [65, 0], {charset: "A"});
  let (entry = WSP.WSP_WELL_KNOWN_CHARSETS["utf-8"]) {
    wsp_decode_test(WSP.AcceptCharsetValue, [entry.number | 0x80], {charset: entry.name});
  }
  // Test for Accept-Charset-General-Form
  wsp_decode_test(WSP.AcceptCharsetValue, [1, 128], {charset: "*"});
  let (entry = WSP.WSP_WELL_KNOWN_CHARSETS["utf-8"]) {
    wsp_decode_test(WSP.AcceptCharsetValue, [2, 1, entry.number], {charset: entry.name});
    wsp_decode_test(WSP.AcceptCharsetValue, [1, entry.number | 0x80], {charset: entry.name});
  }
  wsp_decode_test(WSP.AcceptCharsetValue, [3, 65, 0, 100], {charset: "A", q: 0.99});

  run_next_test();
});

//
// Test target: WellKnownCharset
//

//// WellKnownCharset.decode ////

add_test(function test_WellKnownCharset_decode() {
  wsp_decode_test(WSP.WellKnownCharset, [0xFF], null, "NotWellKnownEncodingError");
  // Test for Any-Charset
  wsp_decode_test(WSP.WellKnownCharset, [128], {charset: "*"});
  // Test for number-typed return value from IntegerValue
  wsp_decode_test(WSP.WellKnownCharset, [1, 3], {charset: "ansi_x3.4-1968"});
  // Test for array-typed return value from IntegerValue
  wsp_decode_test(WSP.WellKnownCharset, [7, 0, 0, 0, 0, 0, 0, 0, 3], null, "CodeError");

  run_next_test();
});

//
// Test target: ContentTypeValue
//

//// ContentTypeValue.decodeConstrainedMedia ////

add_test(function test_ContentTypeValue_decodeConstrainedMedia() {
  // Test for string-typed return value from ConstrainedEncoding
  wsp_decode_test_ex(function (data) {
      return WSP.ContentTypeValue.decodeConstrainedMedia(data);
    }, [65, 0], {media: "a", params: null}
  );
  // Test for number-typed return value from ConstrainedEncoding
  wsp_decode_test_ex(function (data) {
      return WSP.ContentTypeValue.decodeConstrainedMedia(data);
    }, [0x33 | 0x80], {media: "application/vnd.wap.multipart.related", params: null}
  );
  // Test for NotWellKnownEncodingError
  wsp_decode_test_ex(function (data) {
      return WSP.ContentTypeValue.decodeConstrainedMedia(data);
    }, [0x80], null, "NotWellKnownEncodingError"
  );

  run_next_test();
});

//// ContentTypeValue.decodeMedia ////

add_test(function test_ContentTypeValue_decodeMedia() {
  // Test for NullTerminatedTexts
  wsp_decode_test_ex(function (data) {
      return WSP.ContentTypeValue.decodeMedia(data);
    }, [65, 0], "a"
  );
  // Test for IntegerValue
  wsp_decode_test_ex(function (data) {
      return WSP.ContentTypeValue.decodeMedia(data);
    }, [0x3E | 0x80], "application/vnd.wap.mms-message"
  );
  wsp_decode_test_ex(function (data) {
      return WSP.ContentTypeValue.decodeMedia(data);
    }, [0x80], null, "NotWellKnownEncodingError"
  );

  run_next_test();
});

//// ContentTypeValue.decodeMediaType ////

add_test(function test_ContentTypeValue_decodeMediaType() {
  wsp_decode_test_ex(function (data) {
      return WSP.ContentTypeValue.decodeMediaType(data, 1);
    }, [0x3E | 0x80],
    {media: "application/vnd.wap.mms-message", params: null}
  );
  wsp_decode_test_ex(function (data) {
      return WSP.ContentTypeValue.decodeMediaType(data, 14);
    }, [0x3E | 0x80, 1, 0x19, 60, 115, 109, 105, 108, 62, 0, 65, 0, 66, 0],
    {media: "application/vnd.wap.mms-message", params: {start: "<smil>", a: "B"}}
  );

  run_next_test();
});

//// ContentTypeValue.decodeContentGeneralForm ////

add_test(function test_ContentTypeValue_decodeContentGeneralForm() {
  wsp_decode_test_ex(function (data) {
      return WSP.ContentTypeValue.decodeContentGeneralForm(data);
    }, [14, 0x3E | 0x80, 1, 0x19, 60, 115, 109, 105, 108, 62, 0, 65, 0, 66, 0],
    {media: "application/vnd.wap.mms-message", params: {start: "<smil>", a: "B"}}
  );

  run_next_test();
});

//// ContentTypeValue.decode ////

add_test(function test_ContentTypeValue_decode() {
  wsp_decode_test(WSP.ContentTypeValue,
    [14, 0x3E | 0x80, 1, 0x19, 60, 115, 109, 105, 108, 62, 0, 65, 0, 66, 0],
    {media: "application/vnd.wap.mms-message", params: {start: "<smil>", a: "B"}}
  );

  wsp_decode_test(WSP.ContentTypeValue, [0x33 | 0x80],
    {media: "application/vnd.wap.multipart.related", params: null}
  );

  run_next_test();
});

//
// Test target: ApplicationIdValue
//

//// ApplicationIdValue.decode ////

add_test(function test_ApplicationIdValue_decode() {
  wsp_decode_test(WSP.ApplicationIdValue, [0], "");
  wsp_decode_test(WSP.ApplicationIdValue, [65, 0], "A");
  wsp_decode_test(WSP.ApplicationIdValue, [97, 0], "a");
  let (entry = WSP.OMNA_PUSH_APPLICATION_IDS["x-wap-application:mms.ua"]) {
    wsp_decode_test(WSP.ApplicationIdValue, [entry.number | 0x80], entry.urn);
    wsp_decode_test(WSP.ApplicationIdValue, [1, entry.number], entry.urn);
  }
  wsp_decode_test(WSP.ApplicationIdValue, [0xFF], null, "NotWellKnownEncodingError");

  run_next_test();
});

//
// Test target: PduHelper
//

//// PduHelper.parseHeaders ////

add_test(function test_PduHelper_parseHeaders() {
  wsp_decode_test_ex(function (data) {
      return WSP.PduHelper.parseHeaders(data, data.array.length);
    }, [0x80 | 0x05, 2, 0x23, 0x28, 0x80 | 0x2F, 0x80 | 0x04],
    {"age": 9000, "x-wap-application-id": "x-wap-application:mms.ua"}
  );

  run_next_test();
});

