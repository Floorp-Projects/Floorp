/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let WSP = {};
subscriptLoader.loadSubScript("resource://gre/modules/WspPduHelper.jsm", WSP);
WSP.debug = do_print;

function run_test() {
  run_next_test();
}

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
  wsp_decode_test_ex(function(data) {
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
