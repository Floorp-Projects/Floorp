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
  function func(data) {
    return WSP.skipValue(data);
  }

  // Test for zero-valued first octet:
  wsp_decode_test_ex(func, [0], null);
  // Test first octet < 31
  wsp_decode_test_ex(func, [1, 2], [2]);
  // Test first octet = 31
  wsp_decode_test_ex(func, [31, 0], null);
  wsp_decode_test_ex(func, [31, 1, 2], [2]);
  // Test first octet <= 127
  wsp_decode_test_ex(func, strToCharCodeArray("Hello world!"), "Hello world!");
  // Test first octet >= 128
  wsp_decode_test_ex(func, [0x80 | 0x01], 0x01);

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

//// Octet.encodeMultiple ////

add_test(function test_Octet_encodeMultiple() {
  wsp_encode_test_ex(function (data, input) {
    WSP.Octet.encodeMultiple(data, input);
    return data.array;
  }, [0, 1, 2, 3], [0, 1, 2, 3]);

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

//// QuotedString.encode ////

add_test(function test_QuotedString_encode() {
  wsp_encode_test(WSP.QuotedString, "B2G", [34].concat(strToCharCodeArray("B2G")));

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

function LongInteger_decode_testcases(target) {
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
  LongInteger_decode_testcases(WSP.LongInteger);

  run_next_test();
});

//// LongInteger.encode ////

function LongInteger_encode_testcases(target) {
  wsp_encode_test(target, 0x80,           [1, 0x80]);
  wsp_encode_test(target, 0x8002,         [2, 0x80, 2]);
  wsp_encode_test(target, 0x800203,       [3, 0x80, 2, 3]);
  wsp_encode_test(target, 0x80020304,     [4, 0x80, 2, 3, 4]);
  wsp_encode_test(target, 0x8002030405,   [5, 0x80, 2, 3, 4, 5]);
  wsp_encode_test(target, 0x800203040506, [6, 0x80, 2, 3, 4, 5, 6]);
  // Test LongInteger of more than 6 octets
  wsp_encode_test(target, 0x1000000000000, null, "CodeError");
  // Test input empty array
  wsp_encode_test(target, [], null, "CodeError");
  // Test input octets array of length 1..30
  let array = [];
  for (let i = 1; i <= 30; i++) {
    array.push(i);
    wsp_encode_test(target, array, [i].concat(array));
  }
  // Test input octets array of 31 elements.
  array.push(31);
  wsp_encode_test(target, array, null, "CodeError");
}
add_test(function test_LongInteger_encode() {
  wsp_encode_test(WSP.LongInteger, 0, [1, 0]);

  LongInteger_encode_testcases(WSP.LongInteger);

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

//// UintVar.encode ////

add_test(function test_UintVar_encode() {
  // Test up to max 53 bits integer
  wsp_encode_test(WSP.UintVar, 0, [0]);
  wsp_encode_test(WSP.UintVar, 0x7F, [0x7F]);
  wsp_encode_test(WSP.UintVar, 0x3FFF, [0xFF, 0x7F]);
  wsp_encode_test(WSP.UintVar, 0x1FFFFF, [0xFF, 0xFF, 0x7F]);
  wsp_encode_test(WSP.UintVar, 0xFFFFFFF, [0xFF, 0xFF, 0xFF, 0x7F]);
  wsp_encode_test(WSP.UintVar, 0x7FFFFFFFF, [0xFF, 0xFF, 0xFF, 0xFF, 0x7F]);
  wsp_encode_test(WSP.UintVar, 0x3FFFFFFFFFF, [0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F]);
  wsp_encode_test(WSP.UintVar, 0x1FFFFFFFFFFFF, [0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F]);
  wsp_encode_test(WSP.UintVar, 0x1FFFFFFFFFFFFF, [0x8F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F]);

  run_next_test();
});

//
// Test target: ConstrainedEncoding
//

//// ConstrainedEncoding.decode ////

add_test(function test_ConstrainedEncoding_decode() {
  wsp_decode_test(WSP.ConstrainedEncoding, [0x80], 0);
  wsp_decode_test(WSP.ConstrainedEncoding, [32, 0], " ");

  run_next_test();
});

//// ConstrainedEncoding.encode ////

add_test(function test_ConstrainedEncoding_encode() {
  wsp_encode_test(WSP.ConstrainedEncoding, 0, [0x80 | 0]);
  wsp_encode_test(WSP.ConstrainedEncoding, "A", [65, 0]);

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

//// ValueLength.encode ////

add_test(function test_ValueLength_encode() {
  for (let i = 0; i < 256; i++) {
    if (i < 31) {
      wsp_encode_test(WSP.ValueLength, i, [i]);
    } else if (i < 128) {
      wsp_encode_test(WSP.ValueLength, i, [31, i]);
    } else {
      wsp_encode_test(WSP.ValueLength, i, [31, (0x80 | (i / 128)), i % 128]);
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

//// NoValue.encode ////

add_test(function test_NoValue_encode() {
  wsp_encode_test(WSP.NoValue, undefined, [0]);
  wsp_encode_test(WSP.NoValue, null, [0]);
  wsp_encode_test(WSP.NoValue, 0, null, "CodeError");
  wsp_encode_test(WSP.NoValue, "", null, "CodeError");
  wsp_encode_test(WSP.NoValue, [], null, "CodeError");
  wsp_encode_test(WSP.NoValue, {}, null, "CodeError");

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

//// TextValue.encode ////

add_test(function test_TextValue_encode() {
  wsp_encode_test(WSP.TextValue, undefined, [0]);
  wsp_encode_test(WSP.TextValue, null, [0]);
  wsp_encode_test(WSP.TextValue, "", [0]);
  wsp_encode_test(WSP.TextValue, "A", [65, 0]);
  wsp_encode_test(WSP.TextValue, "\x80", [34, 128, 0]);

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

  LongInteger_decode_testcases(WSP.IntegerValue);

  run_next_test();
});

//// IntegerValue.decode ////

add_test(function test_IntegerValue_encode() {
  for (let i = 0; i < 128; i++) {
    wsp_encode_test(WSP.IntegerValue, i, [0x80 | i]);
  }

  LongInteger_encode_testcases(WSP.IntegerValue);

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

//// DateValue.encode ////

add_test(function test_DateValue_encode() {
  wsp_encode_test(WSP.DateValue, new Date(0x80 * 1000), [1, 0x80]);

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

//// QValue.encode ////

add_test(function test_QValue_encode() {
  wsp_encode_test(WSP.QValue, 0, [1]);
  wsp_encode_test(WSP.QValue, 0.99, [100]);
  wsp_encode_test(WSP.QValue, 0.001, [101]);
  wsp_encode_test(WSP.QValue, 0.999, [0x88, 0x4B]);
  wsp_encode_test(WSP.QValue, 1, null, "CodeError");

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

//// VersionValue.encode ////

add_test(function test_VersionValue_encode() {
  for (let major = 1; major < 8; major++) {
    let version = (major << 4) | 0x0F;
    wsp_encode_test(WSP.VersionValue, version, [0x80 | version]);

    for (let minor = 0; minor < 15; minor++) {
      version = (major << 4) | minor;
      wsp_encode_test(WSP.VersionValue, version, [0x80 | version]);
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
// Test target: TypeValue
//

//// TypeValue.decode ////

add_test(function test_TypeValue_decode() {
  // Test for string-typed return value from ConstrainedEncoding
  wsp_decode_test(WSP.TypeValue, [65, 0], "a");
  // Test for number-typed return value from ConstrainedEncoding
  wsp_decode_test(WSP.TypeValue, [0x33 | 0x80],
                  "application/vnd.wap.multipart.related");
  wsp_decode_test(WSP.TypeValue, [0x1d | 0x80], "image/gif");
  // Test for NotWellKnownEncodingError
  wsp_decode_test(WSP.TypeValue, [0x59 | 0x80], null, "NotWellKnownEncodingError");

  run_next_test();
});

//// TypeValue.encode ////

add_test(function test_TypeValue_encode() {
  wsp_encode_test(WSP.TypeValue, "no/such.type",
                  [110, 111, 47, 115, 117, 99, 104, 46, 116, 121, 112, 101, 0]);
  wsp_encode_test(WSP.TypeValue, "application/vnd.wap.multipart.related",
                  [0x33 | 0x80]);
  wsp_encode_test(WSP.TypeValue, "image/gif",
                  [0x1d | 0x80]);

  run_next_test();
});

//
// Test target: Parameter
//

//// Parameter.decodeTypedParameter ////

add_test(function test_Parameter_decodeTypedParameter() {
  function func(data) {
    return WSP.Parameter.decodeTypedParameter(data);
  }

  // Test for array-typed return value from IntegerValue
  wsp_decode_test_ex(func, [7, 0, 0, 0, 0, 0, 0, 0], null, "CodeError");
  // Test for number-typed return value from IntegerValue
  wsp_decode_test_ex(func, [1, 0, 0], {name: "q", value: null});
  // Test for NotWellKnownEncodingError
  wsp_decode_test_ex(func, [1, 0xFF], null, "NotWellKnownEncodingError");
  // Test for parameter specific decoder
  wsp_decode_test_ex(func, [1, 0, 100], {name: "q", value: 0.99});
  // Test for TextValue
  wsp_decode_test_ex(func, [1, 0x10, 48, 46, 57, 57, 0],
                     {name: "secure", value: "0.99"});
  // Test for TextString
  wsp_decode_test_ex(func, [1, 0x0A, 60, 115, 109, 105, 108, 62, 0],
                     {name: "start", value: "<smil>"});
  // Test for skipValue
  wsp_decode_test_ex(func, [1, 0x0A, 128], null);

  run_next_test();
});

//// Parameter.decodeUntypedParameter ////

add_test(function test_Parameter_decodeUntypedParameter() {
  function func (data) {
    return WSP.Parameter.decodeUntypedParameter(data);
  }

  wsp_decode_test_ex(func, [1], null, "CodeError");
  wsp_decode_test_ex(func, [65, 0, 0], {name: "a", value: null});
  // Test for IntegerValue
  wsp_decode_test_ex(func, [65, 0, 1, 0], {name: "a", value: 0});
  // Test for TextValue
  wsp_decode_test_ex(func, [65, 0, 66, 0], {name: "a", value: "B"});

  run_next_test();
});

//// Parameter.decode ////

add_test(function test_Parameter_decode() {
  wsp_decode_test(WSP.Parameter, [1, 0x0A, 60, 115, 109, 105, 108, 62, 0],
                  {name: "start", value: "<smil>"});
  wsp_decode_test(WSP.Parameter, [65, 0, 66, 0], {name: "a", value: "B"});

  run_next_test();
});

//// Parameter.decodeMultiple ////

add_test(function test_Parameter_decodeMultiple() {
  wsp_decode_test_ex(function (data) {
      return WSP.Parameter.decodeMultiple(data, 13);
    }, [1, 0x0A, 60, 115, 109, 105, 108, 62, 0, 65, 0, 66, 0], {start: "<smil>", a: "B"}
  );

  run_next_test();
});

//// Parameter.encodeTypedParameter ////

add_test(function test_Parameter_encodeTypedParameter() {
  function func(data, input) {
    WSP.Parameter.encodeTypedParameter(data, input);
    return data.array;
  }

  // Test for NotWellKnownEncodingError
  wsp_encode_test_ex(func, {name: "xxx", value: 0}, null, "NotWellKnownEncodingError");
  wsp_encode_test_ex(func, {name: "q", value: 0}, [0x80, 1]);
  wsp_encode_test_ex(func, {name: "name", value: "A"}, [0x85, 65, 0]);

  run_next_test();
});

//// Parameter.encodeUntypedParameter ////

add_test(function test_Parameter_encodeUntypedParameter() {
  function func(data, input) {
    WSP.Parameter.encodeUntypedParameter(data, input);
    return data.array;
  }

  wsp_encode_test_ex(func, {name: "q", value: 0}, [113, 0, 0x80]);
  wsp_encode_test_ex(func, {name: "name", value: "A"}, [110, 97, 109, 101, 0, 65, 0]);

  run_next_test();
});

//// Parameter.encodeMultiple ////

add_test(function test_Parameter_encodeMultiple() {
  function func(data, input) {
    WSP.Parameter.encodeMultiple(data, input);
    return data.array;
  }

  wsp_encode_test_ex(func, {q: 0, n: "A"}, [0x80, 1, 110, 0, 65, 0]);

  run_next_test();
});

//// Parameter.encode ////

add_test(function test_Parameter_encode() {

  wsp_encode_test(WSP.Parameter, {name: "q", value: 0}, [0x80, 1]);
  wsp_encode_test(WSP.Parameter, {name: "n", value: "A"}, [110, 0, 65, 0]);

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

//// FieldName.encode ////

add_test(function test_FieldName_encode() {
  wsp_encode_test(WSP.FieldName, "", [0]);
  wsp_encode_test(WSP.FieldName, "date", [0x92]);

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

//// AcceptCharsetValue.encodeAnyCharset ////

add_test(function test_AcceptCharsetValue_encodeAnyCharset() {
  function func(data, input) {
    WSP.AcceptCharsetValue.encodeAnyCharset(data, input);
    return data.array;
  }

  wsp_encode_test_ex(func, null, [0x80]);
  wsp_encode_test_ex(func, undefined, [0x80]);
  wsp_encode_test_ex(func, {}, [0x80]);
  wsp_encode_test_ex(func, {charset: null}, [0x80]);
  wsp_encode_test_ex(func, {charset: "*"}, [0x80]);
  wsp_encode_test_ex(func, {charset: "en"}, null, "CodeError");

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
  wsp_decode_test(WSP.WellKnownCharset, [1, 3], {charset: "us-ascii"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 4], {charset: "iso-8859-1"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 5], {charset: "iso-8859-2"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 6], {charset: "iso-8859-3"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 7], {charset: "iso-8859-4"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 8], {charset: "iso-8859-5"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 9], {charset: "iso-8859-6"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 10], {charset: "iso-8859-7"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 11], {charset: "iso-8859-8"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 12], {charset: "iso-8859-9"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 13], {charset: "iso-8859-10"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 17], {charset: "shift_jis"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 18], {charset: "euc-jp"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 37], {charset: "iso-2022-kr"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 38], {charset: "euc-kr"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 39], {charset: "iso-2022-jp"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 40], {charset: "iso-2022-jp-2"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 81], {charset: "iso-8859-6-e"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 82], {charset: "iso-8859-6-i"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 84], {charset: "iso-8859-8-e"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 85], {charset: "iso-8859-8-i"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 1000], {charset: "iso-10646-ucs-2"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 1015], {charset: "utf-16"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 2025], {charset: "gb2312"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 2026], {charset: "big5"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 2084], {charset: "koi8-r"});
  wsp_decode_test(WSP.WellKnownCharset, [1, 2252], {charset: "windows-1252"});
  wsp_decode_test(WSP.WellKnownCharset, [2, 3, 247], {charset: "utf-16"});
  // Test for array-typed return value from IntegerValue
  wsp_decode_test(WSP.WellKnownCharset, [7, 0, 0, 0, 0, 0, 0, 0, 3], null, "CodeError");

  run_next_test();
});

//// WellKnownCharset.encode ////

add_test(function test_WellKnownCharset_encode() {
  // Test for Any-charset
  wsp_encode_test(WSP.WellKnownCharset, {charset: "*"}, [0x80]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "us-ascii"}, [128 + 3]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "iso-8859-1"}, [128 + 4]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "iso-8859-2"}, [128 + 5]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "iso-8859-3"}, [128 + 6]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "iso-8859-4"}, [128 + 7]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "iso-8859-5"}, [128 + 8]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "iso-8859-6"}, [128 + 9]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "iso-8859-7"}, [128 + 10]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "iso-8859-8"}, [128 + 11]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "iso-8859-9"}, [128 + 12]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "iso-8859-10"}, [128 + 13]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "shift_jis"}, [128 + 17]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "euc-jp"}, [128 + 18]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "iso-2022-kr"}, [128 + 37]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "euc-kr"}, [128 + 38]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "iso-2022-jp"}, [128 + 39]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "iso-2022-jp-2"}, [128 + 40]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "iso-8859-6-e"}, [128 + 81]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "iso-8859-6-i"}, [128 + 82]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "iso-8859-8-e"}, [128 + 84]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "iso-8859-8-i"}, [128 + 85]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "UTF-8"}, [128 + 106]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "iso-10646-ucs-2"}, [2, 0x3, 0xe8]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "UTF-16"}, [2, 0x3, 0xf7]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "gb2312"}, [2, 0x7, 0xe9]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "big5"}, [2, 0x7, 0xea]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "koi8-r"}, [2, 0x8, 0x24]);
  wsp_encode_test(WSP.WellKnownCharset, {charset: "windows-1252"}, [2, 0x8, 0xcc]);

  run_next_test();
});

//
// Test target: ContentTypeValue
//

//// ContentTypeValue.decodeConstrainedMedia ////

add_test(function test_ContentTypeValue_decodeConstrainedMedia() {
  function func(data) {
    return WSP.ContentTypeValue.decodeConstrainedMedia(data);
  }

  // Test for string-typed return value from ConstrainedEncoding
  wsp_decode_test_ex(func, [65, 0], {media: "a", params: null});
  // Test for number-typed return value from ConstrainedEncoding
  for(let ix = 0; ix <WSP.WSP_WELL_KNOWN_CONTENT_TYPES.length ; ++ix){
    wsp_decode_test_ex(func, [WSP.WSP_WELL_KNOWN_CONTENT_TYPES[ix].number | 0x80],
      {media: WSP.WSP_WELL_KNOWN_CONTENT_TYPES[ix].value, params: null});
  }
  // Test for NotWellKnownEncodingError
  wsp_decode_test_ex(func, [0x59 | 0x80], null, "NotWellKnownEncodingError");

  run_next_test();
});

//// ContentTypeValue.decodeMedia ////

add_test(function test_ContentTypeValue_decodeMedia() {
  function func(data) {
    return WSP.ContentTypeValue.decodeMedia(data);
  }

  // Test for NullTerminatedTexts
  wsp_decode_test_ex(func, [65, 0], "a");
  // Test for IntegerValue
  wsp_decode_test_ex(func, [0x3E | 0x80], "application/vnd.wap.mms-message");
  wsp_decode_test_ex(func, [0x59 | 0x80], null, "NotWellKnownEncodingError");

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
    }, [0x3E | 0x80, 1, 0x0A, 60, 115, 109, 105, 108, 62, 0, 65, 0, 66, 0],
    {media: "application/vnd.wap.mms-message", params: {start: "<smil>", a: "B"}}
  );

  run_next_test();
});

//// ContentTypeValue.decodeContentGeneralForm ////

add_test(function test_ContentTypeValue_decodeContentGeneralForm() {
  wsp_decode_test_ex(function (data) {
      return WSP.ContentTypeValue.decodeContentGeneralForm(data);
    }, [14, 0x3E | 0x80, 1, 0x0A, 60, 115, 109, 105, 108, 62, 0, 65, 0, 66, 0],
    {media: "application/vnd.wap.mms-message", params: {start: "<smil>", a: "B"}}
  );

  run_next_test();
});

//// ContentTypeValue.decode ////

add_test(function test_ContentTypeValue_decode() {
  wsp_decode_test(WSP.ContentTypeValue,
    [14, 0x3E | 0x80, 1, 0x0A, 60, 115, 109, 105, 108, 62, 0, 65, 0, 66, 0],
    {media: "application/vnd.wap.mms-message", params: {start: "<smil>", a: "B"}}
  );

  wsp_decode_test(WSP.ContentTypeValue, [0x33 | 0x80],
    {media: "application/vnd.wap.multipart.related", params: null}
  );

  run_next_test();
});

//// ContentTypeValue.encodeConstrainedMedia ////

add_test(function test_ContentTypeValue_encodeConstrainedMedia() {
  function func(data, input) {
    WSP.ContentTypeValue.encodeConstrainedMedia(data, input);
    return data.array;
  }

  // Test media type with additional parameters.
  wsp_encode_test_ex(func, {media: "a", params: [{a: "b"}]}, null, "CodeError");
  wsp_encode_test_ex(func, {media: "no/such.type"},
                     [110, 111, 47, 115, 117, 99, 104, 46, 116, 121, 112, 101, 0]);
  for(let ix = 0; ix <WSP.WSP_WELL_KNOWN_CONTENT_TYPES.length ; ++ix){
    wsp_encode_test_ex(func, {media: WSP.WSP_WELL_KNOWN_CONTENT_TYPES[ix].value},
    [WSP.WSP_WELL_KNOWN_CONTENT_TYPES[ix].number | 0x80]);
  }
  wsp_encode_test_ex(func, {media: "TexT/X-hdml"}, [0x04 | 0x80]);
  wsp_encode_test_ex(func, {media: "appLication/*"}, [0x10 | 0x80]);

  run_next_test();
});

//// ContentTypeValue.encodeMediaType ////

add_test(function test_ContentTypeValue_encodeMediaType() {
  function func(data, input) {
    WSP.ContentTypeValue.encodeMediaType(data, input);
    return data.array;
  }

  wsp_encode_test_ex(func, {media: "no/such.type"},
                     [110, 111, 47, 115, 117, 99, 104, 46, 116, 121, 112, 101, 0]);
  wsp_encode_test_ex(func, {media: "application/vnd.wap.multipart.related"},
                     [0x33 | 0x80]);
  wsp_encode_test_ex(func, {media: "a", params: {b: "c", q: 0}},
                     [97, 0, 98, 0, 99, 0, 128, 1]);

  run_next_test();
});

//// ContentTypeValue.encodeContentGeneralForm ////

add_test(function test_ContentTypeValue_encodeContentGeneralForm() {
  function func(data, input) {
    WSP.ContentTypeValue.encodeContentGeneralForm(data, input);
    return data.array;
  }

  wsp_encode_test_ex(func, {media: "a", params: {b: "c", q: 0}},
                     [8, 97, 0, 98, 0, 99, 0, 128, 1]);

  run_next_test();
});

//// ContentTypeValue.encode ////

add_test(function test_ContentTypeValue_encode() {
  wsp_encode_test(WSP.ContentTypeValue, {media: "no/such.type"},
                  [110, 111, 47, 115, 117, 99, 104, 46, 116, 121, 112, 101, 0]);
  wsp_encode_test(WSP.ContentTypeValue,
                  {media: "application/vnd.wap.multipart.related"},
                  [0x33 | 0x80]);
  wsp_encode_test(WSP.ContentTypeValue, {media: "a", params: {b: "c", q: 0}},
                  [8, 97, 0, 98, 0, 99, 0, 128, 1]);

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

//// PduHelper.decodeStringContent ////

add_test(function StringContent_decode() {
  //Test for utf-8
  let (entry = WSP.WSP_WELL_KNOWN_CHARSETS["utf-8"]) {
    // "Mozilla" in full width.
    let str = "\uff2d\uff4f\uff5a\uff49\uff4c\uff4c\uff41";

    let conv = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
               .createInstance(Ci.nsIScriptableUnicodeConverter);
    conv.charset = entry.converter;

    let raw = conv.convertToByteArray(str);
    let data = {array: raw, offset: 0};
    let octetArray = WSP.Octet.decodeMultiple(data, data.array.length);
    wsp_decode_test_ex(function (data) {
        return WSP.PduHelper.decodeStringContent(data.array, "utf-8");
      }, octetArray, str);
  }

  let (entry = WSP.WSP_WELL_KNOWN_CHARSETS["utf-16"]) {
    // "Mozilla" in full width.
    let str = "\u004d\u006F\u007A\u0069\u006C\u006C\u0061";

    let conv = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
               .createInstance(Ci.nsIScriptableUnicodeConverter);
    conv.charset = entry.converter;

    let raw = conv.convertToByteArray(str);
    let data = {array: raw, offset: 0};
    let octetArray = WSP.Octet.decodeMultiple(data, data.array.length);
    wsp_decode_test_ex(function (data) {
        return WSP.PduHelper.decodeStringContent(data.array, "utf-16");
      }, raw, str);
  }

  run_next_test();
});
