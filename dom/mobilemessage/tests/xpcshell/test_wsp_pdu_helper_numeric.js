/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let WSP = {};
subscriptLoader.loadSubScript("resource://gre/modules/WspPduHelper.jsm", WSP);
WSP.debug = do_print;

function run_test() {
  run_next_test();
}

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
  wsp_decode_test_ex(function(data) {
      return WSP.Octet.decodeMultiple(data, 3);
    }, [0, 1, 2], [0, 1, 2], null
  );
  wsp_decode_test_ex(function(data) {
      return WSP.Octet.decodeMultiple(data, 3);
    }, new Uint8Array([0, 1, 2]), new Uint8Array([0, 1, 2]), null
  );
  wsp_decode_test_ex(function(data) {
      return WSP.Octet.decodeMultiple(data, 4);
    }, [0, 1, 2], null, "RangeError"
  );

  run_next_test();
});

//// Octet.decodeEqualTo ////

add_test(function test_Octet_decodeEqualTo() {
  wsp_decode_test_ex(function(data) {
      return WSP.Octet.decodeEqualTo(data, 1);
    }, [1], 1, null
  );
  wsp_decode_test_ex(function(data) {
      return WSP.Octet.decodeEqualTo(data, 2);
    }, [1], null, "CodeError"
  );
  wsp_decode_test_ex(function(data) {
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
  wsp_encode_test_ex(function(data, input) {
    WSP.Octet.encodeMultiple(data, input);
    return data.array;
  }, [0, 1, 2, 3], [0, 1, 2, 3]);

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
