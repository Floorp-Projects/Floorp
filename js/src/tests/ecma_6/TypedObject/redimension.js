// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 922172;
var summary = 'redimension method';

/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var T = TypedObject;

function runTests() {
  var counter;

  // create an array of 40 bytes with some initial data
  var Bytes40 = new T.ArrayType(T.uint8, 40);
  var bytes40 = new Bytes40();
  for (var i = 0, counter = 0; i < 40; i++)
    bytes40[i] = counter++;

  // redimension to an array of 10x4 bytes, check data is unchanged
  var Bytes10times4 = new T.ArrayType(new T.ArrayType(T.uint8, 4), 10);
  var bytes10times4 = bytes40.redimension(Bytes10times4);
  counter = 0;
  for (var i = 0; i < 10; i++)
    for (var j = 0; j < 4; j++)
      assertEq(counter++, bytes10times4[i][j]);

  // redimension to an array of 2x5x2x2, check data is unchanged
  var Bytes2times5times2times2 =
    new T.ArrayType(
      new T.ArrayType(
        new T.ArrayType(
          new T.ArrayType(T.uint8, 2), 2), 5), 2);
  var bytes2times5times2times2 = bytes10times4.redimension(Bytes2times5times2times2);
  counter = 0;
  for (var i = 0; i < 2; i++)
    for (var j = 0; j < 5; j++)
      for (var k = 0; k < 2; k++)
        for (var l = 0; l < 2; l++)
          assertEq(counter++, bytes2times5times2times2[i][j][k][l]);

  // test what happens if number of elements does not match
  assertThrowsInstanceOf(() => {
    var Bytes10times5 = new T.ArrayType(new T.ArrayType(T.uint8, 5), 10);
    bytes40.redimension(Bytes10times5);
  }, TypeError, "New number of elements does not match old number of elements");

  // test what happens if inner type does not match, even if size is the same
  assertThrowsInstanceOf(() => {
    var Words40 = new T.ArrayType(T.uint8Clamped, 40);
    bytes40.redimension(Words40);
  }, TypeError, "New element type is not equivalent to old element type");

  reportCompare(true, true);
  print("Tests complete");
}

runTests();


