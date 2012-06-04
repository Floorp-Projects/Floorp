/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;


let subscriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
                        .getService(Ci.mozIJSSubScriptLoader);

/**
 * Test whether specified function throws exception with expected
 * result.
 *
 * @param func
 *        Function to be tested.
 * @param exception
 *        Expected class name of thrown exception. Use null for no throws.
 * @param stack
 *        Optional stack object to be printed. null for Components#stack#caller.
 */
function do_check_throws(func, result, stack)
{
  if (!stack)
    stack = Components.stack.caller;

  try {
    func();
  } catch (ex) {
    if (ex.name == result) {
      return;
    }
    do_throw("expected result " + result + ", caught " + ex, stack);
  }

  if (result) {
    do_throw("expected result " + result + ", none thrown", stack);
  }
}

/**
 * Internal test function for comparing results.
 *
 * @param func
 *        A function under test. It should accept an arguement and return the
 *        result.
 * @param data
 *        Input data for `func`.
 * @param expect
 *        Expected result.
 */
function wsp_test_func(func, data, expect) {
  let result_str = JSON.stringify(func(data));
  let expect_str = JSON.stringify(expect);
  if (result_str !== expect_str) {
    do_throw("expect decoded value: '" + expect_str + "', got '" + result_str + "'");
  }
}

/**
 * Test customized WSP PDU decoding.
 *
 * @param func
 *        Decoding func under test. It should return a decoded value if invoked.
 * @param input
 *        Array of octets as test data.
 * @param expect
 *        Expected decoded value, use null if expecting errors instead.
 * @param exception
 *        Expected class name of thrown exception. Use null for no throws.
 */
function wsp_decode_test_ex(func, input, expect, exception) {
  let data = {array: input, offset: 0};
  do_check_throws(wsp_test_func.bind(null, func, data, expect), exception);
}

/**
 * Test default WSP PDU decoding.
 *
 * @param target
 *        Target decoding object, ie. TextValue.
 * @param input
 *        Array of octets as test data.
 * @param expect
 *        Expected decoded value, use null if expecting errors instead.
 * @param exception
 *        Expected class name of thrown exception. Use null for no throws.
 */
function wsp_decode_test(target, input, expect, exception) {
  let func = function decode_func(data) {
    return target.decode(data);
  };

  wsp_decode_test_ex(func, input, expect, exception);
}

/**
 * Test customized WSP PDU encoding.
 *
 * @param func
 *        Encoding func under test. It should return an encoded octet array if
 *        invoked.
 * @param input
 *        An object to be encoded.
 * @param expect
 *        Expected encoded octet array, use null if expecting errors instead.
 * @param exception
 *        Expected class name of thrown exception. Use null for no throws.
 */
function wsp_encode_test_ex(func, input, expect, exception) {
  let data = {array: [], offset: 0};
  do_check_throws(wsp_test_func.bind(null, func.bind(null, data), input,
                                     expect), exception);
}

/**
 * Test default WSP PDU encoding.
 *
 * @param target
 *        Target decoding object, ie. TextValue.
 * @param input
 *        An object to be encoded.
 * @param expect
 *        Expected encoded octet array, use null if expecting errors instead.
 * @param exception
 *        Expected class name of thrown exception. Use null for no throws.
 */
function wsp_encode_test(target, input, expect, exception) {
  let func = function encode_func(data, input) {
    target.encode(data, input);

    // Remove extra space consumed during encoding.
    while (data.array.length > data.offset) {
      data.array.pop();
    }

    return data.array;
  }

  wsp_encode_test_ex(func, input, expect, exception);
}

/**
 * @param str
 *        A string.
 * @param noAppendNull
 *        True to omit terminating NUL octet. Default false.
 *
 * @return A number array of char codes of characters in `str`.
 */
function strToCharCodeArray(str, noAppendNull) {
  let result = [];

  for (let i = 0; i < str.length; i++) {
    result.push(str.charCodeAt(i));
  }
  if (!noAppendNull) {
    result.push(0);
  }

  return result;
}

