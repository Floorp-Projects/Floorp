// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 922115;
var summary = 'check we cannot create handles to unsized arrays';

var T = TypedObject;

function runTests() {
  var Line = new T.StructType({from: T.uint8, to: T.uint8});
  var Lines = Line.array();
  assertThrowsInstanceOf(function() Lines.handle(), TypeError,
                         "was able to create handle to unsized array");

  reportCompare(true, true);
  print("Tests complete");
}

runTests();


