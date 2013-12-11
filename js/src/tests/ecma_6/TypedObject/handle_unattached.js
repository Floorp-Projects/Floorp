// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var BUGNUMBER = 898342;
var summary = 'Unattached handles';

var T = TypedObject;

function runTests() {
  var Line = new T.StructType({from: T.uint8, to: T.uint8});
  var Lines = Line.array(3);

  // Create unattached handle to array, struct:
  var handle = Lines.handle();
  var handle0 = Line.handle();

  // Accessing length throws:
  assertThrowsInstanceOf(function() handle.length, TypeError,
                         "handle.length did not yield error");

  // Accessing properties throws:
  assertThrowsInstanceOf(function() handle[0], TypeError,
                         "Unattached handle did not yield error");
  assertThrowsInstanceOf(function() handle0.from, TypeError,
                         "Unattached handle did not yield error");

  // Handle.get() throws:
  assertThrowsInstanceOf(function() T.Handle.get(handle), TypeError,
                         "Unattached handle did not yield error");
  assertThrowsInstanceOf(function() T.Handle.get(handle0), TypeError,
                         "Unattached handle did not yield error");

  // Handle.set() throws:
  assertThrowsInstanceOf(function() T.Handle.set(handle, [{},{},{}]), TypeError,
                         "Unattached handle did not yield error");
  assertThrowsInstanceOf(function() T.Handle.set(handle0, {}), TypeError,
                         "Unattached handle did not yield error");

  reportCompare(true, true);
  print("Tests complete");
}

runTests();


