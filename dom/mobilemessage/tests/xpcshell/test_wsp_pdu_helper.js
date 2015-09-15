/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var WSP = {};
subscriptLoader.loadSubScript("resource://gre/modules/WspPduHelper.jsm", WSP);
WSP.debug = do_print;

function run_test() {
  run_next_test();
}

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
// Test target: AcceptCharsetValue
//

//// AcceptCharsetValue.decode ////

add_test(function test_AcceptCharsetValue_decode() {
  wsp_decode_test(WSP.AcceptCharsetValue, [0xFF], null, "CodeError");
  // Test for Any-Charset
  wsp_decode_test(WSP.AcceptCharsetValue, [128], {charset: "*"});
  // Test for Constrained-Charset
  wsp_decode_test(WSP.AcceptCharsetValue, [65, 0], {charset: "A"});
  let entry = WSP.WSP_WELL_KNOWN_CHARSETS["utf-8"];
  wsp_decode_test(WSP.AcceptCharsetValue, [entry.number | 0x80], {charset: entry.name});

  // Test for Accept-Charset-General-Form
  wsp_decode_test(WSP.AcceptCharsetValue, [1, 128], {charset: "*"});
  entry = WSP.WSP_WELL_KNOWN_CHARSETS["utf-8"];
  wsp_decode_test(WSP.AcceptCharsetValue, [2, 1, entry.number], {charset: entry.name});
  wsp_decode_test(WSP.AcceptCharsetValue, [1, entry.number | 0x80], {charset: entry.name});
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
  for (let ix = 0; ix <WSP.WSP_WELL_KNOWN_CONTENT_TYPES.length ; ++ix) {
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
  wsp_decode_test_ex(function(data) {
      return WSP.ContentTypeValue.decodeMediaType(data, 1);
    }, [0x3E | 0x80],
    {media: "application/vnd.wap.mms-message", params: null}
  );
  wsp_decode_test_ex(function(data) {
      return WSP.ContentTypeValue.decodeMediaType(data, 14);
    }, [0x3E | 0x80, 1, 0x0A, 60, 115, 109, 105, 108, 62, 0, 65, 0, 66, 0],
    {media: "application/vnd.wap.mms-message", params: {start: "<smil>", a: "B"}}
  );

  run_next_test();
});

//// ContentTypeValue.decodeContentGeneralForm ////

add_test(function test_ContentTypeValue_decodeContentGeneralForm() {
  wsp_decode_test_ex(function(data) {
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
  for (let ix = 0; ix <WSP.WSP_WELL_KNOWN_CONTENT_TYPES.length ; ++ix) {
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
  let entry = WSP.OMNA_PUSH_APPLICATION_IDS["x-wap-application:mms.ua"];
  wsp_decode_test(WSP.ApplicationIdValue, [entry.number | 0x80], entry.urn);
  wsp_decode_test(WSP.ApplicationIdValue, [1, entry.number], entry.urn);
  wsp_decode_test(WSP.ApplicationIdValue, [0xFF], null, "NotWellKnownEncodingError");

  run_next_test();
});
